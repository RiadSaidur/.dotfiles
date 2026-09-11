/*
 * shadow-cat — sassy independent Hyprland desktop pet
 * Procedural Cairo silhouette. Chases only on its own terms.
 */

#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <cairo.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SIZE 96
#define MAX_WINS 24
#define EDGE_PAD 10   /* sit just outside window chrome / content */
#define N_ANGLES 16   /* facing quantized to 22.5° steps */

/* Movement / gait styles — 12+ */
typedef enum {
    M_WALK = 0,      /* 1  steady walk */
    M_CREEP,         /* 2  slow prowl */
    M_TROT,          /* 3  bouncy trot */
    M_SPRINT,        /* 4  flat-out dash */
    M_DIAGONAL,      /* 5  commit to a diagonal heading */
    M_ZIGZAG,        /* 6  weave left/right */
    M_ARC,           /* 7  curved approach */
    M_CIRCLE,        /* 8  orbit in place / loop */
    M_BACKPEDAL,     /* 9  reverse away */
    M_SIDESTEP,      /* 10 crab / lateral */
    M_HOP,           /* 11 hop-steps */
    M_SKID,          /* 12 slide / skid stop feel */
    M_SPIN,          /* 13 spin-turn then go */
    M_FIGURE8,       /* 14 figure-8 saunter */
    M_PAUSING,       /* 15 stop-start stalk */
    M_COUNT
} Motion;

/* Behaviours (mood / activity) */
typedef enum {
    B_LOAF = 0,
    B_SLEEP,
    B_WANDER,
    B_STRETCH,
    B_GROOM,
    B_LOOK,
    B_TAILFLICK,
    B_STARE,
    B_SCRATCH,       /* clawing — 4 styles */
    B_DASH,
    B_CORNER,
    B_FLOP,          /* roll — 4 styles */
    B_JUMP,          /* jump — 4 styles */
    B_SIDE_EYE,
    B_HISS,
    B_ACCEPT,
    B_COUNT
} Behavior;

/* Stunt style indices (jump / roll / scratch) */
enum {
    JUMP_POP = 0,    /* vertical pop */
    JUMP_LEAP,       /* long leap */
    JUMP_TWIST,      /* spin in air */
    JUMP_DOUBLE,     /* double bounce */
    JUMP_STYLES = 4,

    ROLL_SIDE = 0,   /* side tumble */
    ROLL_SOMERSAULT, /* forward flip */
    ROLL_BARREL,     /* dizzy spins */
    ROLL_LOG,        /* slow fat log roll */
    ROLL_STYLES = 4,

    SCR_HUNT = 0,    /* chase cursor, claw */
    SCR_POST,        /* rear + vertical scratch */
    SCR_DIG,         /* floor dig */
    SCR_FURY,        /* furious blur scratch */
    SCR_STYLES = 4
};

typedef struct {
    int x, y, w, h;
} WinRect;

typedef struct {
    GtkWidget *win;
    GtkWidget *da;
    int ipc_fd;

    double x, y;
    double cx, cy;
    double tx, ty;
    double angle;        /* radians, desired head direction */
    double angle_vis;    /* smoothed draw angle */
    int angle_i;         /* 0..N_ANGLES-1 (for look sweeps) */
    double facing;
    double vx, vy;       /* velocity for fluid motion */
    double frame;
    double bob;
    double t;
    double dur;
    double blink_t;
    double mood;
    double path_u;
    double ox, oy;
    double amp;
    double spin_left;
    double dist0;        /* start distance to target (for ease) */
    int eyes_closed;
    int scratch_hits;
    int anim_style;      /* style variant for jump/roll/scratch */
    double air_z;        /* visual jump height (px up) */
    double twist;        /* extra spin radians for stunts */
    Behavior beh;
    Behavior prev_beh;
    Motion motion;
    int after_beh;
    double after_dur;
    double blend_t;      /* seconds since behavior change */
    double blend_dur;
    /* Smoothed pose channels (0..1 or continuous) — lerp toward targets */
    double pose_sit;
    double pose_roll;
    double pose_sx;
    double pose_sy;
    double pose_walk;
    double pose_sleep;
    double pose_groom;
    double pose_hiss;
    double pose_pet;
    double pose_narrow;
    double pose_bob;     /* bob amplitude */
    double pose_wag;
    double head_drop;    /* groom head lower */
    double pose_fat;     /* body roundness */
    double pose_long;    /* body length stretch */
    double pose_arch;    /* back arch */
    double pose_puff;    /* angry/scared fluff */
    double pose_head;    /* head scale */
    guint tick_id;
    int mon_w, mon_h, mon_x, mon_y;
    int need_fast;
    int workspace_id;
    int obscured;        /* hidden while a fullscreen app is focused */
    double fade;         /* 0..1 draw opacity (soft reappear) */
    WinRect wins[MAX_WINS];
    int nwins;
} Cat;

static Cat g;

static void set_beh(Behavior b, double dur);
static void pick_sleep_side(void);
static void apply_margins(void);
static double frand(void);

/* ---------- Hyprland IPC (cheap) ---------- */

static int ipc_connect(void)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!rt || !sig)
        return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", rt, sig);
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, strlen(path) + 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int ipc_request(const char *cmd, char *out, size_t out_sz)
{
    if (g.ipc_fd < 0) {
        g.ipc_fd = ipc_connect();
        if (g.ipc_fd < 0)
            return -1;
    }
    size_t len = strlen(cmd);
    if (write(g.ipc_fd, cmd, len) != (ssize_t)len) {
        close(g.ipc_fd);
        g.ipc_fd = ipc_connect();
        if (g.ipc_fd < 0)
            return -1;
        if (write(g.ipc_fd, cmd, len) != (ssize_t)len)
            return -1;
    }
    size_t got = 0;
    while (got + 1 < out_sz) {
        ssize_t n = read(g.ipc_fd, out + got, out_sz - 1 - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            close(g.ipc_fd);
            g.ipc_fd = -1;
            return -1;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    close(g.ipc_fd);
    g.ipc_fd = -1;
    out[got] = '\0';
    return (int)got;
}

static int read_cursor(double *ox, double *oy)
{
    char buf[64];
    if (ipc_request("cursorpos", buf, sizeof(buf)) < 0)
        return 0;
    int x = 0, y = 0;
    if (sscanf(buf, "%d, %d", &x, &y) != 2 && sscanf(buf, "%d,%d", &x, &y) != 2)
        return 0;
    *ox = (double)x;
    *oy = (double)y;
    return 1;
}

static void refresh_monitor_size(void)
{
    char buf[4096];
    if (ipc_request("monitors", buf, sizeof(buf)) < 0)
        return;
    int w = 0, h = 0, ox = 0, oy = 0;
    char *line = strstr(buf, "Monitor ");
    if (line) {
        char *tab = strchr(line, '\n');
        if (tab)
            sscanf(tab + 1, "\t%dx%d", &w, &h);
    }
    char *at = strstr(buf, " at ");
    if (at)
        sscanf(at, " at %dx%d", &ox, &oy);
    char *foc = strstr(buf, "focused: yes");
    if (foc) {
        char *p = foc;
        while (p > buf && strncmp(p, "Monitor ", 8) != 0)
            p--;
        if (strncmp(p, "Monitor ", 8) == 0) {
            char *tab = strchr(p, '\n');
            if (tab)
                sscanf(tab + 1, "\t%dx%d", &w, &h);
            char *at2 = strstr(p, " at ");
            char *end = strstr(p + 1, "Monitor ");
            if (at2 && (!end || at2 < end))
                sscanf(at2, " at %dx%d", &ox, &oy);
        }
    }
    if (w > 0 && h > 0) {
        g.mon_w = w;
        g.mon_h = h;
        g.mon_x = ox;
        g.mon_y = oy;
    }
}

static void clamp_pos(void)
{
    double min_x = g.mon_x + 4;
    double min_y = g.mon_y + 4;
    double max_x = g.mon_x + g.mon_w - SIZE - 4;
    double max_y = g.mon_y + g.mon_h - SIZE - 4;
    if (g.x < min_x) g.x = min_x;
    if (g.y < min_y) g.y = min_y;
    if (g.x > max_x) g.x = max_x;
    if (g.y > max_y) g.y = max_y;
}

static void apply_margins(void)
{
    clamp_pos();
    gtk_layer_set_margin(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_LEFT,
                         (int)lround(g.x) - g.mon_x);
    gtk_layer_set_margin(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_TOP,
                         (int)lround(g.y) - g.mon_y);
}

static double frand(void)
{
    return (double)rand() / (double)RAND_MAX;
}

static double cat_cx(void) { return g.x + SIZE * 0.5; }
static double cat_cy(void) { return g.y + SIZE * 0.55; }

static double angle_diff(double a, double b)
{
    return atan2(sin(a - b), cos(a - b));
}

static void smooth_turn_to(double target, double dt, double turn_rate)
{
    double d = angle_diff(target, g.angle);
    double max_step = turn_rate * dt;
    if (d > max_step) d = max_step;
    if (d < -max_step) d = -max_step;
    g.angle += d;
    /* keep index in sync loosely */
    double step = (2.0 * G_PI) / N_ANGLES;
    int i = (int)lround(g.angle / step);
    i %= N_ANGLES;
    if (i < 0) i += N_ANGLES;
    g.angle_i = i;
    g.facing = (cos(g.angle) >= 0) ? 1.0 : -1.0;
}

static void set_angle_from_vec(double dx, double dy)
{
    if (fabs(dx) + fabs(dy) < 0.01)
        return;
    g.angle = atan2(dy, dx);
    double step = (2.0 * G_PI) / N_ANGLES;
    int i = (int)lround(g.angle / step);
    i %= N_ANGLES;
    if (i < 0) i += N_ANGLES;
    g.angle_i = i;
    g.facing = (cos(g.angle) >= 0) ? 1.0 : -1.0;
}

static void face_toward(double x, double y)
{
    set_angle_from_vec(x - cat_cx(), y - cat_cy());
}

static void pick_motion_for(Behavior b)
{
    static const Motion roam[] = {
        M_WALK, M_CREEP, M_TROT, M_DIAGONAL, M_ZIGZAG, M_ARC,
        M_SIDESTEP, M_HOP, M_FIGURE8, M_PAUSING, M_BACKPEDAL, M_SKID,
        M_CIRCLE, M_SPIN, M_SPRINT
    };
    static const Motion dash[] = {
        M_SPRINT, M_DIAGONAL, M_ZIGZAG, M_HOP, M_SKID, M_ARC, M_SPIN
    };
    static const Motion corner[] = {
        M_WALK, M_CREEP, M_TROT, M_ARC, M_PAUSING, M_SIDESTEP
    };
    const Motion *tab = roam;
    int n = (int)(sizeof(roam) / sizeof(roam[0]));
    if (b == B_DASH) {
        tab = dash;
        n = (int)(sizeof(dash) / sizeof(dash[0]));
    } else if (b == B_CORNER) {
        tab = corner;
        n = (int)(sizeof(corner) / sizeof(corner[0]));
    } else if (b == B_SCRATCH) {
        g.motion = (frand() < 0.5) ? M_SPRINT : M_HOP;
        g.amp = 18 + frand() * 22;
        g.path_u = 0;
        g.ox = cat_cx();
        g.oy = cat_cy();
        g.spin_left = 0;
        g.dist0 = 0;
        return;
    }
    g.motion = tab[rand() % n];
    g.amp = 14 + frand() * 36;
    g.path_u = 0;
    g.ox = cat_cx();
    g.oy = cat_cy();
    g.spin_left = (g.motion == M_SPIN) ? (0.7 + frand() * 1.2) : 0;
    g.dist0 = 0;
    if (g.motion == M_CIRCLE)
        g.amp = 28 + frand() * 40;
}

static double motion_speed(void)
{
    switch (g.motion) {
    case M_CREEP:    return 70.0;
    case M_WALK:     return 130.0;
    case M_TROT:     return 190.0;
    case M_SPRINT:   return 340.0;
    case M_DIAGONAL: return 160.0;
    case M_ZIGZAG:   return 150.0;
    case M_ARC:      return 140.0;
    case M_CIRCLE:   return 120.0;
    case M_BACKPEDAL:return 100.0;
    case M_SIDESTEP: return 115.0;
    case M_HOP:      return 200.0;
    case M_SKID:     return 250.0;
    case M_SPIN:     return 145.0;
    case M_FIGURE8:  return 135.0;
    case M_PAUSING:  return 120.0;
    default:         return 130.0;
    }
}

static void apply_velocity(double dt)
{
    g.x += g.vx * dt;
    g.y += g.vy * dt;
    apply_margins();
}

/* Blend current velocity toward desired (px/s). */
static void steer(double wish_x, double wish_y, double max_spd, double dt, double accel)
{
    double wlen = hypot(wish_x, wish_y);
    double tx = 0, ty = 0;
    if (wlen > 1e-6) {
        tx = (wish_x / wlen) * max_spd;
        ty = (wish_y / wlen) * max_spd;
    }
    g.vx += (tx - g.vx) * fmin(1.0, accel * dt);
    g.vy += (ty - g.vy) * fmin(1.0, accel * dt);
}

/* Styled fluid step toward target. Returns 1 if arrived. */
static int move_styled(double ttx, double tty, double dt)
{
    double max_spd = motion_speed();
    double turn = 7.5; /* rad/s */

    if (g.motion == M_SPIN && g.spin_left > 0) {
        g.spin_left -= dt;
        smooth_turn_to(g.angle + ((g.facing > 0) ? 1.0 : -1.0) * 4.0, dt, 10.0);
        g.vx *= (1.0 - 4.0 * dt);
        g.vy *= (1.0 - 4.0 * dt);
        apply_velocity(dt);
        g.frame += dt * 14.0;
        return 0;
    }

    if (g.motion == M_CIRCLE || g.motion == M_FIGURE8) {
        g.path_u += dt * (g.motion == M_CIRCLE ? 0.85 : 0.45);
        double u = g.path_u * 2.0 * G_PI;
        double nx, ny;
        if (g.motion == M_CIRCLE) {
            nx = g.ox + cos(u) * g.amp;
            ny = g.oy + sin(u) * g.amp;
        } else {
            nx = g.ox + sin(u) * g.amp;
            ny = g.oy + sin(u) * cos(u) * g.amp * 0.6;
        }
        /* chase the path point with steering (no teleport) */
        double px = nx - cat_cx();
        double py = ny - cat_cy();
        steer(px, py, max_spd * 1.15, dt, 8.0);
        if (hypot(g.vx, g.vy) > 8.0)
            smooth_turn_to(atan2(g.vy, g.vx), dt, turn);
        apply_velocity(dt);
        g.frame += dt * 9.0;
        return (g.path_u >= 1.0) ? 1 : 0;
    }

    double dx = ttx - cat_cx();
    double dy = tty - cat_cy();
    double dist = hypot(dx, dy);
    if (g.dist0 <= 1.0)
        g.dist0 = fmax(dist, 40.0);

    if (dist < 10.0 && hypot(g.vx, g.vy) < 25.0) {
        g.vx *= 0.5;
        g.vy *= 0.5;
        if (dist < 6.0) {
            g.vx = 0;
            g.vy = 0;
            return 1;
        }
    }

    double ux = (dist > 1e-6) ? dx / dist : 0;
    double uy = (dist > 1e-6) ? dy / dist : 0;
    double px = -uy;
    double py = ux;
    double wish_x = ux;
    double wish_y = uy;
    double accel = 5.5;

    /* ease in/out along the trip */
    double progress = 1.0 - dist / g.dist0;
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    double ease = sin(progress * G_PI); /* slow-fast-slow feel via speed scale below */
    double spd = max_spd * (0.55 + 0.45 * ease);
    /* stronger ease-out near end */
    if (dist < 120.0)
        spd *= fmax(0.25, dist / 120.0);

    if (g.motion == M_PAUSING) {
        /* soft breath in motion instead of hard freeze */
        double pulse = 0.5 + 0.5 * sin(g.t * 5.0);
        spd *= 0.15 + 0.85 * pulse * pulse;
        accel = 3.5;
    } else if (g.motion == M_BACKPEDAL) {
        smooth_turn_to(atan2(dy, dx), dt, turn);
        wish_x = -cos(g.angle);
        wish_y = -sin(g.angle);
        /* still drift toward side target */
        wish_x = wish_x * 0.7 + ux * 0.3;
        wish_y = wish_y * 0.7 + uy * 0.3;
    } else if (g.motion == M_SIDESTEP) {
        smooth_turn_to(atan2(dy, dx), dt, turn);
        wish_x = -sin(g.angle) * 0.7 + ux * 0.3;
        wish_y = cos(g.angle) * 0.7 + uy * 0.3;
    } else if (g.motion == M_ZIGZAG) {
        double wiggle = sin(g.t * 5.5) * 0.55;
        wish_x = ux + px * wiggle;
        wish_y = uy + py * wiggle;
    } else if (g.motion == M_ARC) {
        double bend = sin(progress * G_PI) * 0.75 * ((g.amp > 25) ? 1.0 : -1.0);
        wish_x = ux + px * bend;
        wish_y = uy + py * bend;
    } else if (g.motion == M_DIAGONAL) {
        /* bias toward 45° lanes but keep continuous */
        double a = atan2(uy, ux);
        double oct = G_PI / 4.0;
        double snapped = round(a / oct) * oct;
        a = a + angle_diff(snapped, a) * 0.65;
        wish_x = cos(a);
        wish_y = sin(a);
    } else if (g.motion == M_HOP) {
        double hop = 0.4 + 0.6 * fabs(sin(g.t * 9.0));
        spd *= hop;
        accel = 10.0;
    } else if (g.motion == M_SKID) {
        accel = (dist < 100.0) ? 2.2 : 7.0;
        if (dist < 100.0)
            spd *= 0.5 + 0.5 * (dist / 100.0);
    } else if (g.motion == M_CREEP) {
        accel = 3.0;
        turn = 4.0;
    } else if (g.motion == M_SPRINT) {
        accel = 9.0;
        turn = 9.0;
    } else if (g.motion == M_TROT) {
        accel = 7.0;
        spd *= 0.9 + 0.1 * sin(g.t * 12.0);
    }

    steer(wish_x, wish_y, spd, dt, accel);

    if (hypot(g.vx, g.vy) > 12.0) {
        if (g.motion == M_BACKPEDAL)
            smooth_turn_to(atan2(dy, dx), dt, turn);
        else
            smooth_turn_to(atan2(g.vy, g.vx), dt, turn);
    }

    apply_velocity(dt);
    g.frame += dt * (5.0 + hypot(g.vx, g.vy) * 0.03);
    return 0;
}

static void refresh_workspace(void)
{
    char buf[512];
    if (ipc_request("j/activeworkspace", buf, sizeof(buf)) < 0)
        return;
    char *id = strstr(buf, "\"id\":");
    if (id)
        g.workspace_id = atoi(id + 5);
}

static void refresh_windows(void)
{
    static char buf[192 * 1024];
    g.nwins = 0;
    refresh_workspace();
    if (ipc_request("j/clients", buf, sizeof(buf)) < 0)
        return;

    char *p = buf;
    while (g.nwins < MAX_WINS && (p = strstr(p, "\"address\"")) != NULL) {
        char *end = strstr(p + 1, "\"address\"");
        if (!end)
            end = p + strlen(p);
        /* slice object */
        char *mapped = strstr(p, "\"mapped\"");
        char *hidden = strstr(p, "\"hidden\"");
        char *at = strstr(p, "\"at\"");
        char *size = strstr(p, "\"size\"");
        char *ws = strstr(p, "\"workspace\"");
        if (!mapped || mapped > end) { p = end; continue; }
        if (!at || at > end || !size || size > end) { p = end; continue; }

        int is_mapped = 0, is_hidden = 0, wx = 0, wy = 0, ww = 0, wh = 0, wid = -1;
        {
            char tmp[16];
            if (sscanf(mapped, "\"mapped\": %15s", tmp) == 1)
                is_mapped = (tmp[0] == 't');
        }
        if (hidden && hidden < end) {
            char tmp[16];
            if (sscanf(hidden, "\"hidden\": %15s", tmp) == 1)
                is_hidden = (tmp[0] == 't');
        }
        if (sscanf(at, "\"at\": [ %d , %d", &wx, &wy) != 2 &&
            sscanf(at, "\"at\": [%d, %d", &wx, &wy) != 2) {
            p = end;
            continue;
        }
        if (sscanf(size, "\"size\": [ %d , %d", &ww, &wh) != 2 &&
            sscanf(size, "\"size\": [%d, %d", &ww, &wh) != 2) {
            p = end;
            continue;
        }
        if (ws && ws < end) {
            char *widp = strstr(ws, "\"id\":");
            if (widp && widp < end)
                wid = atoi(widp + 5);
        }

        if (!is_mapped || is_hidden || ww < 80 || wh < 80) {
            p = end;
            continue;
        }
        if (g.workspace_id > 0 && wid > 0 && wid != g.workspace_id) {
            p = end;
            continue;
        }
        /* keep windows that overlap our monitor */
        if (wx + ww < g.mon_x || wy + wh < g.mon_y ||
            wx > g.mon_x + g.mon_w || wy > g.mon_y + g.mon_h) {
            p = end;
            continue;
        }

        g.wins[g.nwins].x = wx;
        g.wins[g.nwins].y = wy;
        g.wins[g.nwins].w = ww;
        g.wins[g.nwins].h = wh;
        g.nwins++;
        p = end;
    }
}

static void pick_screen_edge_target(void)
{
    /* Mostly left/right sides; rare top/bottom */
    int side;
    double r = frand();
    if (r < 0.42)
        side = 2; /* left */
    else if (r < 0.84)
        side = 3; /* right */
    else if (r < 0.92)
        side = 0; /* top */
    else
        side = 1; /* bottom */

    double band = 28.0 + frand() * 48.0;
    double m = 8.0;
    if (side == 0) {
        g.tx = g.mon_x + m + frand() * (g.mon_w - SIZE - 2 * m);
        g.ty = g.mon_y + m + 28 + frand() * band;
    } else if (side == 1) {
        g.tx = g.mon_x + m + frand() * (g.mon_w - SIZE - 2 * m);
        g.ty = g.mon_y + g.mon_h - SIZE - m - frand() * band;
    } else if (side == 2) {
        g.tx = g.mon_x + m + frand() * band;
        g.ty = g.mon_y + 40 + frand() * (g.mon_h - SIZE - 60);
    } else {
        g.tx = g.mon_x + g.mon_w - SIZE - m - frand() * band;
        g.ty = g.mon_y + 40 + frand() * (g.mon_h - SIZE - 60);
    }
}

static void pick_window_edge_target(const WinRect *w)
{
    /* Prefer vertical window edges (sides), rarely top/bottom chrome */
    int side = (frand() < 0.78) ? ((frand() < 0.5) ? 2 : 3) : (rand() % 2);
    double along;
    if (side == 0) {
        along = frand();
        g.tx = w->x + along * (w->w - SIZE);
        g.ty = w->y - SIZE + EDGE_PAD;
    } else if (side == 1) {
        along = frand();
        g.tx = w->x + along * (w->w - SIZE);
        g.ty = w->y + w->h - EDGE_PAD;
    } else if (side == 2) {
        along = frand();
        g.tx = w->x - SIZE + EDGE_PAD;
        g.ty = w->y + along * (w->h - SIZE);
    } else {
        along = frand();
        g.tx = w->x + w->w - EDGE_PAD;
        g.ty = w->y + along * (w->h - SIZE);
    }
}

static void pick_content_target(const WinRect *w)
{
    /* Rare mid-window visit — still avoid dead center a bit */
    g.tx = w->x + w->w * (0.15 + frand() * 0.7) - SIZE * 0.5;
    g.ty = w->y + w->h * (0.15 + frand() * 0.7) - SIZE * 0.5;
}

static void pick_wander_target(void)
{
    refresh_windows();
    double roll = frand();

    /* ~2% content, ~22% window sides, rest screen sides */
    if (g.nwins > 0 && roll < 0.02)
        pick_content_target(&g.wins[rand() % g.nwins]);
    else if (g.nwins > 0 && roll < 0.24)
        pick_window_edge_target(&g.wins[rand() % g.nwins]);
    else
        pick_screen_edge_target();

    {
        double sx = g.x, sy = g.y;
        g.x = g.tx;
        g.y = g.ty;
        clamp_pos();
        g.tx = g.x;
        g.ty = g.y;
        g.x = sx;
        g.y = sy;
    }
}

static int active_is_fullscreen(void)
{
    char buf[4096];
    if (ipc_request("j/activewindow", buf, sizeof(buf)) < 0)
        return 0;
    char *fs = strstr(buf, "\"fullscreen\":");
    if (!fs)
        return 0;
    int v = atoi(fs + 13);
    /* 1 = real fullscreen; ignore maximize (often 2) / none (0) */
    return v == 1;
}

static void update_fullscreen_visibility(void)
{
    int fs = active_is_fullscreen();
    if (fs && !g.obscured) {
        g.obscured = 1;
        g.fade = 0;
        gtk_widget_hide(g.win);
    } else if (!fs && g.obscured) {
        g.obscured = 0;
        g.fade = 0; /* soft fade-in after hide */
        gtk_widget_show_all(g.win);
        apply_margins();
        /* Prefer a calm side loaf after being away */
        pick_sleep_side();
        g.x = g.tx;
        g.y = g.ty;
        apply_margins();
        set_beh(B_LOAF, 2.0 + frand() * 2.0);
    }
}

static void pick_corner(void)
{
    int c = rand() % 4;
    double m = 14;
    /* Prefer left/right bottoms for parking */
    if (frand() < 0.7)
        c = (frand() < 0.5) ? 2 : 3;
    if (c == 0) { g.tx = g.mon_x + m; g.ty = g.mon_y + m + 36; }
    else if (c == 1) { g.tx = g.mon_x + g.mon_w - SIZE - m; g.ty = g.mon_y + m + 36; }
    else if (c == 2) { g.tx = g.mon_x + m; g.ty = g.mon_y + g.mon_h - SIZE - m; }
    else { g.tx = g.mon_x + g.mon_w - SIZE - m; g.ty = g.mon_y + g.mon_h - SIZE - m; }
}

/* Sleep only along left/right screen sides (not center, not top bar). */
static void pick_sleep_side(void)
{
    double m = 10;
    double top = g.mon_y + 48;
    double bot = g.mon_y + g.mon_h - SIZE - 12;
    double span = bot - top;
    if (span < 40)
        span = 40;
    g.ty = top + frand() * span;
    if (frand() < 0.5)
        g.tx = g.mon_x + m;
    else
        g.tx = g.mon_x + g.mon_w - SIZE - m;
}

static int is_locomotion(Behavior b)
{
    return b == B_WANDER || b == B_DASH || b == B_CORNER ||
           (b == B_SCRATCH && g.anim_style == SCR_HUNT);
}

static int is_stunt(Behavior b)
{
    return b == B_JUMP || b == B_FLOP || b == B_SCRATCH;
}

static void pick_stunt_style(Behavior b)
{
    if (b == B_JUMP)
        g.anim_style = rand() % JUMP_STYLES;
    else if (b == B_FLOP)
        g.anim_style = rand() % ROLL_STYLES;
    else if (b == B_SCRATCH)
        g.anim_style = rand() % SCR_STYLES;
    else
        g.anim_style = 0;
    g.air_z = 0;
    g.twist = 0;
    g.path_u = 0;
    g.ox = cat_cx();
    g.oy = cat_cy();
}

static double smootherstep(double t)
{
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

static void pose_approach(double *v, double target, double dt, double tau)
{
    double k = 1.0 - exp(-dt / fmax(tau, 0.05));
    *v += (target - *v) * k;
}

static void update_pose_targets(double dt)
{
    double tsit = 0, troll = 0, tsx = 1, tsy = 1;
    double twalk = 0, tsleep = 0, tgroom = 0, thiss = 0, tpet = 0, tnarrow = 0;
    double tbob = 0, twag = 0.1, thead = 0;
    double tfat = 0.16, tlong = 0.48, tarch = 0.0, tpuff = 0.0, ths = 1.0;
    double spd = hypot(g.vx, g.vy);

    switch (g.beh) {
    case B_SLEEP:
        /* soft skinny curl — not a pancake */
        tsit = 4.5; tsleep = 1.0; twag = 0.04; tbob = 0;
        tfat = 0.26; tlong = 0.42; tarch = -0.08; ths = 0.92; tsx = 1.1; tsy = 0.75;
        break;
    case B_LOAF:
        /* slim sit, still soft */
        tsit = 2.4; twag = 0.15; tbob = 0.35;
        tfat = 0.2; tlong = 0.45; ths = 0.98; tsx = 1.04; tsy = 0.9;
        break;
    case B_JUMP: {
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 0;
        if (u > 1) u = 1;
        twalk = 0.35; tbob = 0;
        if (g.anim_style == JUMP_POP) {
            tsx = 0.75 + 0.5 * fabs(sin(u * G_PI));
            tsy = 1.4 - 0.55 * fabs(sin(u * G_PI));
            tfat = 0.2; tlong = 0.35; tarch = 0.4; ths = 1.1;
        } else if (g.anim_style == JUMP_LEAP) {
            tsx = 1.55; tsy = 0.55; tfat = 0.1; tlong = 1.0; tarch = 0.15;
        } else if (g.anim_style == JUMP_TWIST) {
            tsx = 0.85; tsy = 1.2; tfat = 0.25; tlong = 0.45; tarch = 0.5; ths = 1.15;
        } else {
            double peak = (u < 0.5) ? sin(u * 2 * G_PI) : sin((u - 0.5) * 2 * G_PI);
            tsx = 0.8 + 0.4 * fabs(peak);
            tsy = 1.35 - 0.5 * fabs(peak);
            tfat = 0.25; tlong = 0.4;
        }
        if (u > 0.85) { tsx = 1.3; tsy = 0.62; tfat = 0.32; }
        break;
    }
    case B_FLOP: {
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 0;
        if (g.anim_style == ROLL_SIDE) {
            troll = sin(u * G_PI * 2.0) * 1.15;
            tsx = 1.15; tsy = 0.75; tfat = 0.28; tlong = 0.55;
        } else if (g.anim_style == ROLL_SOMERSAULT) {
            troll = 0; tsx = 1.1; tsy = 0.82; tfat = 0.2; tlong = 0.7; tarch = 0.35;
        } else if (g.anim_style == ROLL_BARREL) {
            troll = 0; tfat = 0.25; tlong = 0.45; ths = 1.08; tpuff = 0.3;
        } else {
            troll = sin(u * G_PI) * 0.95;
            tsx = 1.35; tsy = 0.55; tfat = 0.4; tlong = 0.75; tsleep = 0.2;
        }
        tsit = 2.0; twag = 0.25; tbob = 0.6;
        break;
    }
    case B_STRETCH:
        /* rubber-band stretch */
        tsx = 1.7; tsy = 0.45; tsit = 0.5; twag = 0.35;
        tfat = 0.08; tlong = 1.0; tarch = -0.15; ths = 0.82; thead = 0.3;
        break;
    case B_GROOM:
        tgroom = 1.0; thead = 1.0; tsit = 1.6; twag = 0.3;
        tfat = 0.28; tlong = 0.4; tarch = 0.3; tsx = 0.95; tsy = 1.05; ths = 1.02;
        break;
    case B_TAILFLICK:
        twag = 1.15; tbob = 1.2;
        tfat = 0.22; tlong = 0.5; tarch = 0.55; tpuff = 0.3; ths = 1.08;
        break;
    case B_HISS:
        thiss = 1.0; tnarrow = 1.0; twag = 1.0; tbob = 0.8;
        tfat = 0.28; tlong = 0.55; tarch = 0.95; tpuff = 0.85; ths = 1.15;
        tsx = 0.85; tsy = 1.25;
        break;
    case B_ACCEPT:
        tpet = 1.0; twag = 0.7; tbob = 2.6;
        tfat = 0.3; tlong = 0.4; ths = 1.1; tsx = 1.06; tsy = 0.92;
        break;
    case B_SIDE_EYE:
    case B_STARE:
        tnarrow = 1.0; twag = 0.2; tbob = 0.25;
        tfat = 0.18; tlong = 0.45; ths = 1.06; tarch = 0.15;
        break;
    case B_LOOK:
        twag = 0.25; tbob = 0.5;
        tfat = 0.18; tlong = 0.45; ths = 1.1;
        break;
    case B_WANDER:
    case B_DASH:
    case B_CORNER:
        twalk = 1.0;
        twag = 0.45;
        tfat = 0.14; tlong = 0.62; tarch = 0.2; ths = 1.0;
        tbob = (g.motion == M_HOP) ? 6.5 : (g.motion == M_TROT ? 3.2 : 2.0);
        if (g.motion == M_CREEP) {
            tsx = 1.2; tsy = 0.72; tfat = 0.22; tlong = 0.75; tarch = 0.35; tbob = 0.8;
        } else if (g.motion == M_SPRINT || g.beh == B_DASH) {
            tsx = 1.4; tsy = 0.62; tlong = 1.0; tfat = 0.08; tarch = 0.1; twag = 0.7;
        } else if (g.motion == M_HOP) {
            tsx = 0.88; tsy = 1.2; tlong = 0.5; tfat = 0.12;
        } else if (g.motion == M_SKID) {
            tsx = 1.45; tsy = 0.55; tlong = 0.95; tfat = 0.12; tarch = -0.15;
        } else if (g.motion == M_TROT) {
            tsx = 1.12; tsy = 0.88; tlong = 0.65; tfat = 0.14;
        }
        if (spd < 40.0)
            twalk = fmax(0.15, spd / 40.0);
        break;
    case B_SCRATCH: {
        twag = 0.6; tbob = 1.5; twalk = 0.35; tpuff = 0.35; ths = 1.1;
        if (g.anim_style == SCR_HUNT) {
            twalk = 0.8; tlong = 0.7; tarch = 0.55; tsx = 1.2; tsy = 0.8;
        } else if (g.anim_style == SCR_POST) {
            /* reared up tall */
            tsx = 0.7; tsy = 1.55; tarch = 0.7; tfat = 0.25; tlong = 0.3; tsit = -2;
        } else if (g.anim_style == SCR_DIG) {
            tsx = 1.2; tsy = 0.7; tfat = 0.28; tarch = 0.8; tlong = 0.45; thead = 0.7;
        } else { /* FURY */
            tsx = 1.1; tsy = 0.9; tpuff = 0.9; tarch = 0.5; twag = 1.0; tbob = 3.0;
        }
        break;
    }
    default:
        break;
    }

    double b = smootherstep(g.blend_t / fmax(g.blend_dur, 0.05));
    if (!is_locomotion(g.beh) && is_locomotion(g.prev_beh) && b < 1.0) {
        twalk = fmax(twalk, (1.0 - b) * fmin(1.0, spd / 80.0));
        tbob = fmax(tbob, (1.0 - b) * 1.5);
        tlong = fmax(tlong, (1.0 - b) * 0.5);
    }
    if (g.beh == B_SLEEP && b < 1.0) {
        tsit *= b;
        tsleep *= b;
        tfat = tfat * b + 0.18 * (1.0 - b);
        tsit += (1.0 - b) * g.pose_sit * 0.3;
    }
    if (g.prev_beh == B_SLEEP && g.beh != B_SLEEP && b < 1.0) {
        tsleep = fmax(tsleep, 1.0 - b);
        tsit = fmax(tsit, 4.0 * (1.0 - b));
        tfat = fmax(tfat, 0.28 * (1.0 - b));
    }

    double tau = 0.18;
    if (g.beh == B_SLEEP || g.prev_beh == B_SLEEP)
        tau = 0.32;
    if (g.beh == B_HISS || g.beh == B_ACCEPT || g.beh == B_STRETCH)
        tau = 0.11;

    pose_approach(&g.pose_sit, tsit, dt, tau);
    pose_approach(&g.pose_roll, troll, dt, tau * 1.1);
    pose_approach(&g.pose_sx, tsx, dt, tau);
    pose_approach(&g.pose_sy, tsy, dt, tau);
    pose_approach(&g.pose_walk, twalk, dt, 0.15);
    pose_approach(&g.pose_sleep, tsleep, dt, tau);
    pose_approach(&g.pose_groom, tgroom, dt, 0.18);
    pose_approach(&g.pose_hiss, thiss, dt, 0.1);
    pose_approach(&g.pose_pet, tpet, dt, 0.14);
    pose_approach(&g.pose_narrow, tnarrow, dt, 0.16);
    pose_approach(&g.pose_bob, tbob, dt, 0.16);
    pose_approach(&g.pose_wag, twag, dt, 0.14);
    pose_approach(&g.head_drop, thead, dt, 0.16);
    pose_approach(&g.pose_fat, tfat, dt, tau);
    pose_approach(&g.pose_long, tlong, dt, tau);
    pose_approach(&g.pose_arch, tarch, dt, tau);
    pose_approach(&g.pose_puff, tpuff, dt, 0.12);
    pose_approach(&g.pose_head, ths, dt, tau);
    g.blend_t += dt;
}

static void set_beh(Behavior b, double dur)
{
    if (b != g.beh) {
        g.prev_beh = g.beh;
        g.blend_t = 0;
        g.blend_dur = 0.35;
        if (g.prev_beh == B_SLEEP || b == B_SLEEP)
            g.blend_dur = 0.85;
        else if (is_locomotion(g.prev_beh) && !is_locomotion(b))
            g.blend_dur = 0.55; /* coast to a stop */
        else if (!is_locomotion(g.prev_beh) && is_locomotion(b))
            g.blend_dur = 0.4;  /* wind up */
        else if (g.prev_beh == B_HISS || b == B_HISS || b == B_ACCEPT)
            g.blend_dur = 0.25;
    }
    g.beh = b;
    g.t = 0;
    g.dur = dur;
    g.scratch_hits = 0;
    g.dist0 = 0;
    g.need_fast = (is_locomotion(b) || is_stunt(b) || b == B_ACCEPT || b == B_HISS ||
                   b == B_LOOK || b == B_SLEEP || b == B_STRETCH ||
                   b == B_GROOM || b == B_TAILFLICK);
    if (is_stunt(b)) {
        pick_stunt_style(b);
        if (b == B_JUMP) {
            if (g.anim_style == JUMP_LEAP)
                pick_wander_target();
            g.dur = (g.anim_style == JUMP_DOUBLE) ? 1.35 : (0.7 + frand() * 0.45);
            if (dur > 0.1) g.dur = dur;
        } else if (b == B_FLOP) {
            g.dur = (g.anim_style == ROLL_BARREL) ? 1.6 : (1.1 + frand() * 0.7);
            if (dur > 0.1) g.dur = dur;
        } else if (b == B_SCRATCH) {
            g.dur = (g.anim_style == SCR_FURY) ? 1.1 : (1.3 + frand() * 0.9);
            if (dur > 0.1) g.dur = dur;
            if (g.anim_style == SCR_HUNT) {
                pick_motion_for(B_SCRATCH);
                read_cursor(&g.cx, &g.cy);
            }
        }
        g.vx *= 0.5;
        g.vy *= 0.5;
    } else if (is_locomotion(b)) {
        pick_motion_for(b);
        g.vx *= 0.75;
        g.vy *= 0.75;
    } else if (b == B_LOOK) {
        g.motion = M_SPIN;
        g.spin_left = dur;
        g.vx *= 0.5;
        g.vy *= 0.5;
    } else {
        g.vx *= 0.65;
        g.vy *= 0.65;
    }
}

/* Weighted random independent behavior (never auto-chase). */
static void pick_next_behavior(void)
{
    g.after_beh = -1;

    /* Soft mood drifts toward sassy independence */
    g.mood += (frand() - 0.55) * 0.25;
    if (g.mood > 1) g.mood = 1;
    if (g.mood < -1) g.mood = -1;

    /* Heavy on roaming edges; sleep often; rare content via wander roll */
    struct { Behavior b; int w; double dmin, dmax; } opts[] = {
        { B_LOAF,      8, 2.0, 5.0 },
        { B_SLEEP,    12, 6.0, 16.0 },
        { B_WANDER,   26, 2.5, 7.0 },
        { B_STRETCH,   6, 1.2, 2.2 },
        { B_GROOM,     8, 1.5, 3.5 },
        { B_LOOK,      7, 1.0, 2.5 },
        { B_TAILFLICK, 5, 0.8, 1.8 },
        { B_STARE,     5, 2.0, 4.5 },
        { B_SCRATCH,   7, 1.2, 2.4 },
        { B_JUMP,      8, 0.7, 1.4 },
        { B_DASH,      7, 0.8, 1.8 },
        { B_CORNER,    4, 2.0, 4.0 },
        { B_FLOP,      7, 1.0, 2.0 },
        { B_SIDE_EYE,  5, 1.5, 3.5 },
    };
    int n = (int)(sizeof(opts) / sizeof(opts[0]));
    int total = 0;
    for (int i = 0; i < n; i++) {
        int w = opts[i].w;
        if (opts[i].b == B_SCRATCH)
            w += (g.mood < -0.2) ? 3 : -1;
        if (opts[i].b == B_SLEEP && g.mood > 0.2)
            w += 4;
        if (w < 1) w = 1;
        total += w;
        opts[i].w = w;
    }
    int r = rand() % total;
    Behavior pick = B_LOAF;
    double dmin = 3, dmax = 6;
    for (int i = 0; i < n; i++) {
        if (r < opts[i].w) {
            pick = opts[i].b;
            dmin = opts[i].dmin;
            dmax = opts[i].dmax;
            break;
        }
        r -= opts[i].w;
    }

    double dur = dmin + frand() * (dmax - dmin);

    if (pick == B_SLEEP) {
        /* Always nap on a screen side — walk there first */
        pick_sleep_side();
        g.after_beh = B_SLEEP;
        g.after_dur = dur;
        set_beh(B_CORNER, 10.0);
        return;
    }
    if (pick == B_WANDER || pick == B_DASH) {
        pick_wander_target();
        /* After arriving at an edge, often loaf/groom there */
        if (pick == B_WANDER && frand() < 0.55) {
            g.after_beh = (frand() < 0.5) ? B_LOAF : B_GROOM;
            g.after_dur = 2.0 + frand() * 4.0;
        }
    }
    if (pick == B_CORNER) {
        pick_corner();
        if (frand() < 0.65) {
            /* Corner visit may turn into a side nap */
            pick_sleep_side();
            g.after_beh = B_SLEEP;
            g.after_dur = 5.0 + frand() * 10.0;
        }
    }
    if (pick == B_FLOP || pick == B_STRETCH || pick == B_LOAF) {
        /* Stay put only if already peripheral; else scoot to an edge first */
        double cx = cat_cx() - g.mon_x;
        double cy = cat_cy() - g.mon_y;
        (void)cy;
        /* Prefer loafing on left/right sides, not mid-content */
        int near_side = (cx < 150 || cx > g.mon_w - 150);
        if (!near_side && frand() < 0.88) {
            pick_screen_edge_target();
            g.after_beh = pick;
            g.after_dur = dur;
            set_beh(B_WANDER, 8.0);
            return;
        }
    }
    if (pick == B_SCRATCH || pick == B_STARE || pick == B_SIDE_EYE)
        read_cursor(&g.cx, &g.cy);

    set_beh(pick, dur);
}

/* ---------- Drawing ---------- */

static void draw_shadow_blob(cairo_t *cr, double ox, double oy, double sx, double sy)
{
    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, sx, sy);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.02, 0.06, 0.04, 0.22);
    cairo_fill(cr);
    cairo_restore(cr);
}

static void draw_text_fade(cairo_t *cr, const char *s, double x, double y, double a)
{
    if (a <= 0.02)
        return;
    cairo_set_source_rgba(cr, 0.55, 0.78, 0.62, a);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
}

static void draw_cat_body(cairo_t *cr)
{
    const double cx = SIZE * 0.5;
    const double cy = SIZE * 0.58;

    /* Cartoon squash/stretch from hop / pet bounce / jump arc */
    double hop = 0;
    if (g.beh == B_JUMP && g.dur > 0.01) {
        double u = g.t / g.dur;
        hop = (u > 0.08 && u < 0.9) ? sin(u * G_PI) : ((u >= 0.9) ? -0.8 : 0.4);
    } else if (g.motion == M_HOP && g.pose_walk > 0.2) {
        hop = sin(g.frame * 2.4);
    }
    double squash_x = 1.0 + hop * 0.28;
    double squash_y = 1.0 - hop * 0.32;
    if (g.pose_pet > 0.05) {
        double p = sin(g.t * 12.0);
        squash_x *= 1.0 + 0.18 * p * g.pose_pet;
        squash_y *= 1.0 - 0.18 * p * g.pose_pet;
    }

    double bob = sin(g.frame * 2.0) * g.pose_bob;
    if (g.pose_pet > 0.05)
        bob += sin(g.t * 12.0) * 2.4 * g.pose_pet;
    if (g.beh == B_TAILFLICK)
        bob += sin(g.t * 20.0) * 0.8 * smootherstep(fmin(1.0, g.blend_t / 0.25));
    if (hop > 0 && g.beh != B_JUMP)
        bob -= hop * 3.0;

    double fat = g.pose_fat;
    double lng = g.pose_long;
    double arch = g.pose_arch;
    double puff = g.pose_puff;
    double hs = g.pose_head;

    double sx = g.pose_sx * squash_x * (1.0 + puff * 0.18);
    double sy = g.pose_sy * squash_y * (1.0 + puff * 0.12);
    double sit = g.pose_sit;
    double roll = g.pose_roll;

    double diff = angle_diff(g.angle, g.angle_vis);
    g.angle_vis += diff * 0.22;

    cairo_push_group(cr);
    cairo_save(cr);
    cairo_translate(cr, cx, cy + bob + sit - g.air_z);
    cairo_rotate(cr, g.angle_vis + roll + g.twist);
    cairo_scale(cr, sx, sy);

    draw_shadow_blob(cr, 0, 17 + fat + g.air_z * 0.15,
                     11 + fat * 5 + lng * 3 - g.air_z * 0.08,
                     3.2 + fat * 1.5);

    /* Morphable body silhouette (facing +X) */
    double back_y = -10.0 - arch * 10.0 - puff * 3.0;
    /* Soft skinny silhouette — lean body, plush curves */
    double belly_y = 5.5 + fat * 4.5 - lng * 1.5;
    double nose_x = 15.0 + lng * 12.0;
    double butt_x = -13.0 - fat * 2.5 + lng * 1.5;
    double mid_h = (back_y + belly_y) * 0.5;
    double soft = 0.55 + (1.0 - fat) * 0.25; /* control-point softness */

    cairo_new_path(cr);
    cairo_move_to(cr, butt_x, mid_h);
    cairo_curve_to(cr, butt_x - 4 - fat * 2, back_y + 3 * soft,
                   -2, back_y - 1 - fat,
                   6 + lng * 4, back_y);
    cairo_curve_to(cr, 12 + lng * 5, back_y + 2 * soft,
                   nose_x - 1, mid_h - 3,
                   nose_x, mid_h + 1);
    cairo_curve_to(cr, nose_x - 1, belly_y - 1,
                   3, belly_y + 1 + fat,
                   butt_x + 2, belly_y);
    cairo_curve_to(cr, butt_x - 3, belly_y - 2 * soft,
                   butt_x - 4, mid_h + 3 * soft,
                   butt_x, mid_h);
    cairo_close_path(cr);
    /* soft fill — slightly translucent edge feel */
    cairo_set_source_rgba(cr, 0.05, 0.11, 0.09, 0.68);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.03, 0.07, 0.05, 0.28);
    cairo_set_line_width(cr, 1.6);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0.06, 0.13, 0.10, 0.22);
    cairo_set_line_width(cr, 3.2);
    cairo_stroke(cr);

    /* Fluff spikes when puffed */
    if (puff > 0.15) {
        cairo_set_source_rgba(cr, 0.05, 0.12, 0.09, 0.55 * puff);
        for (int i = 0; i < 5; i++) {
            double fx = -6 + i * 5.0;
            double fy = back_y - 1;
            cairo_move_to(cr, fx, fy);
            cairo_line_to(cr, fx + 1.5, fy - 5 - puff * 4);
            cairo_line_to(cr, fx + 3.0, fy);
            cairo_fill(cr);
        }
    }

    /* Head */
    double head_y = -11.0 + 5.0 * g.head_drop - arch * 2.0;
    double head_x = 9.0 + lng * 8.0;
    double hr = (10.2 + fat * 0.8 + puff * 1.8) * hs;
    cairo_save(cr);
    cairo_translate(cr, head_x, head_y);
    cairo_scale(cr, hs * (1.0 + puff * 0.15), hs * (1.0 - g.head_drop * 0.08));
    cairo_arc(cr, 0, 0, hr * 0.95, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.04, 0.10, 0.08, 0.82);
    cairo_fill(cr);

    /* Cheeks when pet / hiss */
    if (g.pose_pet > 0.1 || puff > 0.3) {
        double c = fmax(g.pose_pet, puff * 0.6);
        cairo_set_source_rgba(cr, 0.05, 0.12, 0.09, 0.55 * c);
        cairo_arc(cr, -6, 3, 4.5 + c * 2, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_arc(cr, 6, 3, 4.5 + c * 2, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    /* Ears — tall alert / flat hiss / soft sleep */
    double ear_h = -18.0 - (1.0 - g.pose_sleep) * 4.0 + g.pose_hiss * 10.0;
    double ear_spread = 1.0 + puff * 0.3;
    if (g.pose_hiss > 0.3)
        ear_h = -8.0; /* pinned back visually via lower tip */
    cairo_move_to(cr, -7 * ear_spread, -6);
    cairo_line_to(cr, -4 * ear_spread, ear_h);
    cairo_line_to(cr, -1, -7);
    cairo_close_path(cr);
    cairo_move_to(cr, 1, -7);
    cairo_line_to(cr, 5 * ear_spread, ear_h - 1);
    cairo_line_to(cr, 8 * ear_spread, -5);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, 0.03, 0.09, 0.07, 0.88);
    cairo_fill(cr);

    cairo_move_to(cr, -5.5 * ear_spread, -7);
    cairo_line_to(cr, -4 * ear_spread, ear_h + 5);
    cairo_line_to(cr, -2.5, -7);
    cairo_set_source_rgba(cr, 0.42, 0.75, 0.53, 0.2 + 0.25 * g.pose_hiss);
    cairo_fill(cr);

    /* Eyes in head-local space */
    double sleep_a = g.pose_sleep;
    double narrow = g.pose_narrow;
    if (sleep_a > 0.55 || (g.pose_roll > 0.3 && sleep_a > 0.2)) {
        cairo_set_source_rgba(cr, 0.85, 0.90, 0.88, 0.55 * fmin(1.0, sleep_a + 0.3));
        cairo_set_line_width(cr, 1.8);
        cairo_move_to(cr, -5, 0);
        cairo_curve_to(cr, -2, 3, 0, 3, 2, 0);
        cairo_stroke(cr);
        cairo_move_to(cr, 3, 0);
        cairo_curve_to(cr, 5, 3, 7, 3, 9, 0);
        cairo_stroke(cr);
    } else if (g.eyes_closed) {
        cairo_set_source_rgba(cr, 0.85, 0.90, 0.88, 0.5);
        cairo_set_line_width(cr, 1.5);
        cairo_move_to(cr, -5, 0); cairo_line_to(cr, 0, 0);
        cairo_move_to(cr, 3, 0); cairo_line_to(cr, 8, 0);
        cairo_stroke(cr);
    } else {
        /* big cartoon peepers */
        double er = (3.2 - 1.4 * narrow) * (1.0 + g.pose_pet * 0.2);
        double ex0 = -3.0 - narrow;
        double ex1 = 5.0 + narrow * 0.5;
        double ey = -0.5 - narrow * 0.5;
        cairo_set_source_rgba(cr, 0.78, 0.95, 0.85, 0.65 + 0.2 * g.pose_hiss);
        cairo_arc(cr, ex0, ey, er, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_arc(cr, ex1, ey, er, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.02, 0.05, 0.04, 0.92);
        double px = 0.9 * narrow + (g.beh == B_LOOK ? sin(g.t * 3) * 0.6 : 0);
        cairo_arc(cr, ex0 + px, ey, er * 0.38, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_arc(cr, ex1 + px, ey, er * 0.38, 0, 2 * G_PI);
        cairo_fill(cr);
        /* shiny highlight */
        cairo_set_source_rgba(cr, 0.9, 1.0, 0.95, 0.45);
        cairo_arc(cr, ex0 - er * 0.25, ey - er * 0.25, er * 0.18, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_arc(cr, ex1 - er * 0.25, ey - er * 0.25, er * 0.18, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    /* Tiny cartoon mouth */
    if (g.pose_hiss > 0.2) {
        cairo_set_source_rgba(cr, 0.7, 0.2, 0.25, 0.55 * g.pose_hiss);
        cairo_move_to(cr, 1, 5);
        cairo_curve_to(cr, 3, 9, 6, 9, 8, 5);
        cairo_stroke(cr);
    } else if (g.pose_pet > 0.2) {
        cairo_set_source_rgba(cr, 0.5, 0.75, 0.6, 0.45 * g.pose_pet);
        cairo_arc(cr, 4, 5, 2.2, 0.1, G_PI - 0.1);
        cairo_stroke(cr);
    }

    cairo_restore(cr); /* head */

    /* Tail — longer / springier */
    {
        double wag = g.pose_wag;
        if (g.pose_walk > 0.05)
            wag = g.pose_wag * (0.55 + 0.45 * sin(g.frame * 3.2));
        if (g.beh == B_TAILFLICK)
            wag = g.pose_wag * (0.4 + 0.6 * sin(g.t * 28.0));
        if (g.pose_pet > 0.05)
            wag = fmax(wag, g.pose_wag * (0.5 + 0.5 * sin(g.t * 16.0)));
        if (g.pose_hiss > 0.05) {
            double hw = 0.85 + sin(g.t * 30.0) * 0.25;
            wag = wag * (1.0 - g.pose_hiss) + hw * g.pose_hiss;
        }
        double tx0 = butt_x + 2;
        double tw = 14 + fat * 2 + (g.beh == B_TAILFLICK ? 8 : 0);
        cairo_new_path(cr);
        cairo_move_to(cr, tx0, mid_h);
        cairo_curve_to(cr, tx0 - 10, mid_h - 4 + wag * 12,
                       tx0 - 18 - tw * 0.3, mid_h - 16 + wag * 20,
                       tx0 - 8 - tw * 0.2, mid_h - 22 + wag * 10);
        cairo_curve_to(cr, tx0 - 4, mid_h - 14,
                       tx0 - 2, mid_h - 4,
                       tx0 + 2, mid_h + 2);
        cairo_set_source_rgba(cr, 0.03, 0.09, 0.07, 0.74);
        cairo_fill(cr);
    }

    if (sleep_a > 0.7) {
        draw_text_fade(cr, "z", head_x + 14, head_y - 16, 0.45 * sleep_a);
        draw_text_fade(cr, "z", head_x + 20, head_y - 24, 0.35 * sleep_a);
    }

    if (g.pose_groom > 0.05) {
        cairo_set_source_rgba(cr, 0.04, 0.10, 0.08, 0.85 * g.pose_groom);
        cairo_arc(cr, 2, 4 + sin(g.t * 10) * 3, 5.5, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    /* Exaggerated cartoon legs */
    if (g.pose_walk > 0.08) {
        double s = sin(g.frame * 2.2);
        double leg = 6 + g.pose_walk * 5;
        double thick = 2.8 + (1.0 - lng) * 1.2;
        cairo_set_source_rgba(cr, 0.03, 0.08, 0.06, 0.7 * g.pose_walk);
        cairo_rectangle(cr, -6 + s * 5, belly_y - 2, thick, leg + s * 2);
        cairo_rectangle(cr, 4 - s * 5, belly_y - 2, thick, leg - s * 2);
        cairo_fill(cr);
        /* little feet */
        cairo_rectangle(cr, -7 + s * 5, belly_y - 2 + leg + s * 2, thick + 2, 2.5);
        cairo_rectangle(cr, 3 - s * 5, belly_y - 2 + leg - s * 2, thick + 2, 2.5);
        cairo_fill(cr);
    } else if (fat > 0.35 && g.pose_sleep < 0.5) {
        /* loaf toes peek */
        cairo_set_source_rgba(cr, 0.03, 0.08, 0.06, 0.35 * fat);
        cairo_rectangle(cr, -4, belly_y - 1, 3, 3);
        cairo_rectangle(cr, 2, belly_y - 1, 3, 3);
        cairo_fill(cr);
    }

    if (g.beh == B_SCRATCH && g.t > 0.2) {
        double a = fmin(1.0, g.blend_t / 0.15);
        cairo_set_source_rgba(cr, 0.55, 0.80, 0.62, 0.55 * a);
        cairo_set_line_width(cr, 1.8);
        int lines = (g.anim_style == SCR_FURY) ? 6 : 3;
        for (int i = 0; i < lines; i++) {
            double yo = i * 3.5 - lines;
            if (g.anim_style == SCR_POST) {
                cairo_move_to(cr, nose_x - 4, -18 + i * 5);
                cairo_line_to(cr, nose_x + 6 + sin(g.t * 40 + i) * 2, -28 + i * 5);
            } else if (g.anim_style == SCR_DIG) {
                cairo_move_to(cr, -4 + i * 3, belly_y);
                cairo_line_to(cr, -10 + i * 3 + sin(g.t * 30 + i) * 3, belly_y + 10);
            } else {
                cairo_move_to(cr, nose_x - 2, yo);
                cairo_line_to(cr, nose_x + 12 + sin(g.t * 40 + i) * 3, yo - 2);
            }
            cairo_stroke(cr);
        }
    }

    if (g.pose_pet > 0.05)
        draw_text_fade(cr, "<3", -8, -34 - g.t * 8, 0.45 * g.pose_pet);
    if (g.pose_hiss > 0.1) {
        draw_text_fade(cr, "!", head_x + 12, head_y - 18, 0.75 * g.pose_hiss);
        draw_text_fade(cr, "HSS", head_x + 8, head_y - 6, 0.5 * g.pose_hiss);
    }
    if (g.beh == B_LOOK)
        draw_text_fade(cr, "...", head_x + 12, head_y - 20, 0.4 * smootherstep(g.blend_t / 0.35));

    cairo_restore(cr); /* body transform */
    cairo_pop_group_to_source(cr);
    cairo_paint_with_alpha(cr, g.fade <= 0 ? 0.0 : (g.fade >= 1 ? 1.0 : g.fade));
}

static gboolean on_draw(GtkWidget *w, cairo_t *cr, gpointer data)
{
    (void)w;
    (void)data;
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    draw_cat_body(cr);
    return FALSE;
}

static void queue_draw_cat(void)
{
    gtk_widget_queue_draw(g.da);
}

/* ---------- Interaction: only when you click ---------- */

static gboolean on_button(GtkWidget *w, GdkEventButton *e, gpointer data)
{
    (void)w;
    (void)data;
    if (e->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (e->button == 1) {
        /* Sassy: mood + luck decide accept vs slap */
        double roll = frand() + g.mood * 0.35;
        if (g.beh == B_SLEEP)
            roll -= 0.35; /* don't wake nice */
        if (roll > 0.35) {
            g.mood += 0.15;
            set_beh(B_ACCEPT, 1.0 + frand() * 0.6);
        } else if (roll > 0.05) {
            g.mood -= 0.2;
            set_beh(B_HISS, 0.7 + frand() * 0.5);
        } else {
            g.mood -= 0.25;
            set_beh(B_TAILFLICK, 1.0);
            /* maybe immediately scratch after being annoyed */
            if (frand() < 0.45) {
                /* schedule scratch by short hiss then scratch via dur end */
                set_beh(B_HISS, 0.45);
                g.dur = 0.45; /* after hiss, pick_next may scratch — nudge mood mean */
                g.mood -= 0.15;
            }
        }
        queue_draw_cat();
        return TRUE;
    }
    if (e->button == 2 || e->button == 3) {
        /* poke: send to a side nap, or side-eye */
        if (frand() < 0.5) {
            pick_sleep_side();
            g.after_beh = B_SLEEP;
            g.after_dur = 6.0 + frand() * 8.0;
            set_beh(B_CORNER, 8.0);
        } else {
            set_beh(B_SIDE_EYE, 2.0 + frand() * 2.0);
        }
        queue_draw_cat();
        return TRUE;
    }
    return FALSE;
}

/* ---------- Stunt animations (jump / roll / scratch styles) ---------- */

static void tick_stunt(double dt)
{
    double u = (g.dur > 0.01) ? (g.t / g.dur) : 1.0;
    if (u > 1) u = 1;

    if (g.beh == B_JUMP) {
        double h = 0;
        if (g.anim_style == JUMP_POP) {
            h = sin(u * G_PI) * 54.0;
        } else if (g.anim_style == JUMP_LEAP) {
            h = sin(u * G_PI) * 42.0;
            /* glide toward target */
            move_styled(g.tx + SIZE * 0.5, g.ty + SIZE * 0.55, dt);
        } else if (g.anim_style == JUMP_TWIST) {
            h = sin(u * G_PI) * 48.0;
            g.twist += dt * 14.0;
            g.angle += dt * 14.0;
        } else { /* DOUBLE */
            if (u < 0.5)
                h = sin(u * 2.0 * G_PI) * 40.0;
            else
                h = sin((u - 0.5) * 2.0 * G_PI) * 28.0;
            g.x += cos(g.angle) * 40.0 * dt;
            g.y += sin(g.angle) * 40.0 * dt;
            apply_margins();
        }
        g.air_z = h;
        g.frame += dt * 10.0;
        return;
    }

    if (g.beh == B_FLOP) {
        g.air_z *= exp(-6.0 * dt);
        if (g.anim_style == ROLL_SIDE) {
            double dir = (g.facing >= 0) ? 1.0 : -1.0;
            /* perpendicular tumble */
            g.x += -sin(g.angle) * dir * 110.0 * dt;
            g.y += cos(g.angle) * dir * 110.0 * dt;
            g.twist = u * 2.0 * G_PI * 2.0;
            apply_margins();
        } else if (g.anim_style == ROLL_SOMERSAULT) {
            g.x += cos(g.angle) * 130.0 * dt;
            g.y += sin(g.angle) * 130.0 * dt;
            g.twist = u * 2.0 * G_PI * 2.5; /* forward flips via twist */
            apply_margins();
        } else if (g.anim_style == ROLL_BARREL) {
            g.twist += dt * 16.0;
            g.angle += dt * 16.0;
            /* tiny jitter */
            g.x += cos(g.t * 20.0) * 20.0 * dt;
            g.y += sin(g.t * 17.0) * 20.0 * dt;
            apply_margins();
        } else { /* LOG */
            g.x += cos(g.angle) * 70.0 * dt;
            g.y += sin(g.angle) * 70.0 * dt;
            g.twist = u * G_PI;
            apply_margins();
        }
        g.frame += dt * 8.0;
        return;
    }

    if (g.beh == B_SCRATCH) {
        if (g.anim_style == SCR_HUNT) {
            double dist = hypot(g.cx - cat_cx(), g.cy - cat_cy());
            if (dist > 24.0) {
                move_styled(g.cx, g.cy, dt);
            } else {
                g.scratch_hits++;
                smooth_turn_to(atan2(g.cy - cat_cy(), g.cx - cat_cx()) + sin(g.t * 18.0) * 0.8, dt, 12.0);
                steer(cos(g.angle) * sin(g.t * 20.0), sin(g.angle) * cos(g.t * 16.0), 90.0, dt, 10.0);
                apply_velocity(dt);
                g.air_z = fabs(sin(g.t * 22.0)) * 6.0;
                if (g.scratch_hits > 22)
                    g.t = g.dur;
            }
        } else if (g.anim_style == SCR_POST) {
            /* rear and rake upward */
            g.air_z = 10.0 + sin(g.t * 16.0) * 4.0;
            g.twist = sin(g.t * 10.0) * 0.15;
            g.scratch_hits++;
            g.frame += dt * 14.0;
        } else if (g.anim_style == SCR_DIG) {
            g.air_z = 0;
            g.x += cos(g.angle + G_PI) * sin(g.t * 18.0) * 30.0 * dt;
            g.y += sin(g.angle + G_PI) * sin(g.t * 18.0) * 30.0 * dt;
            apply_margins();
            g.scratch_hits++;
            g.frame += dt * 16.0;
        } else { /* FURY */
            g.air_z = fabs(sin(g.t * 30.0)) * 5.0;
            g.twist = sin(g.t * 40.0) * 0.35;
            g.angle += sin(g.t * 25.0) * dt * 4.0;
            g.scratch_hits++;
            g.frame += dt * 22.0;
        }
    }
}

static gboolean tick(gpointer data)
{
    (void)data;
    static int mon_refresh = 0;
    static guint interval_ms = 60;
    static double cursor_age = 1.0;

    double dt = interval_ms / 1000.0;
    guint want = g.obscured ? 500 : (g.need_fast ? 33 : 120);
    if (want != interval_ms) {
        interval_ms = want;
        g.tick_id = g_timeout_add(interval_ms, tick, NULL);
        return G_SOURCE_REMOVE;
    }

    if (g.obscured || ++mon_refresh > 15) {
        if (!g.obscured)
            refresh_monitor_size();
        update_fullscreen_visibility();
        mon_refresh = 0;
    }

    /* Stay invisible and idle while a fullscreen app has focus */
    if (g.obscured) {
        return G_SOURCE_CONTINUE;
    }

    /* Soft fade-in after returning from fullscreen */
    if (g.fade < 1.0) {
        g.fade = fmin(1.0, g.fade + dt / 0.55);
    }

    /* Poll cursor: sleep needs hover-wake; chase behaviours need it often */
    int wants_cursor = (g.beh == B_SCRATCH || g.beh == B_STARE || g.beh == B_SIDE_EYE ||
                        g.beh == B_HISS || g.beh == B_SLEEP);
    cursor_age += dt;
    if (wants_cursor || cursor_age > 0.9) {
        double nx, ny;
        if (read_cursor(&nx, &ny)) {
            g.cx = nx;
            g.cy = ny;
        }
        cursor_age = 0;
    }

    /* Hover wake while sleeping */
    if (g.beh == B_SLEEP) {
        double dx = g.cx - cat_cx();
        double dy = g.cy - cat_cy();
        if (hypot(dx, dy) < SIZE * 0.55) {
            g.after_beh = -1;
            face_toward(g.cx, g.cy);
            set_beh((frand() < 0.5) ? B_LOOK : B_SIDE_EYE, 1.2 + frand() * 1.5);
            queue_draw_cat();
            return G_SOURCE_CONTINUE;
        }
    }

    g.t += dt;
    g.bob += dt;
    g.blink_t += dt;

    if (is_stunt(g.beh)) {
        tick_stunt(dt);
        update_pose_targets(dt);
        queue_draw_cat();
        if (g.t >= g.dur) {
            g.air_z = 0;
            g.twist = 0;
            if (g.after_beh >= 0) {
                Behavior next = (Behavior)g.after_beh;
                double d = g.after_dur;
                g.after_beh = -1;
                set_beh(next, d);
            } else if (g.beh == B_JUMP)
                set_beh((frand() < 0.5) ? B_LOAF : B_STRETCH, 1.0 + frand());
            else if (g.beh == B_FLOP)
                set_beh((frand() < 0.5) ? B_LOAF : B_LOOK, 1.2 + frand());
            else
                set_beh((frand() < 0.4) ? B_HISS : B_TAILFLICK, 0.8 + frand());
        }
        return G_SOURCE_CONTINUE;
    }

    update_pose_targets(dt);

    /* Coast to a stop when not actively locomoting */
    if (!is_locomotion(g.beh)) {
        double damp = exp(-5.5 * dt);
        g.vx *= damp;
        g.vy *= damp;
        if (hypot(g.vx, g.vy) > 3.0)
            apply_velocity(dt);
        else {
            g.vx = 0;
            g.vy = 0;
        }
    }

    int need_draw = 1; /* pose always easing */

    if (g.blink_t > 3.0 + frand() * 2.0) {
        g.eyes_closed = 1;
        need_draw = 1;
        if (g.blink_t > 3.2) {
            g.eyes_closed = 0;
            g.blink_t = 0;
            need_draw = 1;
        }
    }

    switch (g.beh) {
    case B_LOAF:
    case B_SLEEP:
    case B_FLOP:
        if (((int)(g.bob * 3)) != ((int)((g.bob - dt) * 3)))
            need_draw = 1;
        break;

    case B_LOOK:
        /* continuous head sweep across headings */
        smooth_turn_to(g.angle + 2.2, dt, 3.5);
        need_draw = 1;
        break;

    case B_TAILFLICK:
    case B_STRETCH:
    case B_GROOM:
    case B_SIDE_EYE:
    case B_STARE:
        if (g.beh == B_STARE || g.beh == B_SIDE_EYE)
            smooth_turn_to(atan2(g.cy - cat_cy(), g.cx - cat_cx()), dt, 4.0);
        need_draw = 1;
        break;

    case B_WANDER:
    case B_CORNER:
    case B_DASH:
        if (g.motion == M_CIRCLE || g.motion == M_FIGURE8) {
            if (move_styled(g.tx + SIZE * 0.5, g.ty + SIZE * 0.55, dt))
                g.t = g.dur;
        } else if (move_styled(g.tx + SIZE * 0.5, g.ty + SIZE * 0.55, dt)) {
            g.t = g.dur;
        }
        need_draw = 1;
        break;

    case B_SCRATCH: {
        double dist = hypot(g.cx - cat_cx(), g.cy - cat_cy());
        if (dist > 22.0) {
            move_styled(g.cx, g.cy, dt);
        } else {
            g.scratch_hits++;
            smooth_turn_to(atan2(g.cy - cat_cy(), g.cx - cat_cx()) + sin(g.t * 18.0) * 0.8, dt, 12.0);
            steer(cos(g.angle) * sin(g.t * 20.0), sin(g.angle) * cos(g.t * 16.0), 90.0, dt, 10.0);
            apply_velocity(dt);
            g.frame += dt * 14.0;
            if (g.scratch_hits > 22)
                g.t = g.dur;
        }
        need_draw = 1;
        break;
    }

    case B_ACCEPT:
    case B_HISS:
        need_draw = 1;
        break;

    default:
        break;
    }

    if (g.t >= g.dur) {
        /* Finish settling before hard-switching out of a run */
        int leaving_move = is_locomotion(g.beh);
        if (leaving_move && hypot(g.vx, g.vy) > 35.0) {
            g.vx *= 0.92;
            g.vy *= 0.92;
            need_draw = 1;
        } else if (g.after_beh >= 0) {
            Behavior next = (Behavior)g.after_beh;
            double d = g.after_dur;
            g.after_beh = -1;
            set_beh(next, d);
            need_draw = 1;
        } else if (g.beh == B_HISS && g.mood < -0.35 && frand() < 0.55) {
            read_cursor(&g.cx, &g.cy);
            set_beh(B_SCRATCH, 1.6 + frand());
            need_draw = 1;
        } else if (g.beh == B_ACCEPT && frand() < 0.25) {
            set_beh(B_GROOM, 1.5 + frand());
            need_draw = 1;
        } else if (g.beh == B_SLEEP) {
            /* wake naturally into look/stretch/loaf */
            Behavior wake = B_LOOK;
            double r = frand();
            if (r < 0.34) wake = B_STRETCH;
            else if (r < 0.67) wake = B_LOAF;
            set_beh(wake, 1.5 + frand() * 2.0);
            need_draw = 1;
        } else if (is_locomotion(g.beh)) {
            /* arrive → loaf/groom bridge before next big idea */
            set_beh((frand() < 0.55) ? B_LOAF : B_GROOM, 1.2 + frand() * 2.0);
            need_draw = 1;
        } else {
            pick_next_behavior();
            need_draw = 1;
        }
    }

    if (need_draw)
        queue_draw_cat();
    return G_SOURCE_CONTINUE;
}

static void on_realize(GtkWidget *w, gpointer data)
{
    (void)data;
    GdkScreen *screen = gtk_widget_get_screen(w);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
        gtk_widget_set_visual(w, visual);
    gtk_widget_set_app_paintable(w, TRUE);
}

int main(int argc, char **argv)
{
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    gtk_init(&argc, &argv);

    g.ipc_fd = -1;
    g.facing = 1.0;
    g.angle = 0;
    g.angle_vis = 0;
    g.angle_i = 0;
    g.vx = 0;
    g.vy = 0;
    g.motion = M_WALK;
    g.mood = -0.15;
    g.after_beh = -1;
    g.obscured = 0;
    g.fade = 1.0;
    g.prev_beh = B_LOAF;
    g.blend_t = 1;
    g.blend_dur = 0.01;
    g.pose_sx = 1;
    g.pose_sy = 1;
    g.pose_wag = 0.1;
    g.pose_fat = 0.18;
    g.pose_long = 0.48;
    g.pose_long = 0.35;
    g.pose_head = 1.0;
    g.mon_w = 1920;
    g.mon_h = 1080;

    refresh_monitor_size();
    read_cursor(&g.cx, &g.cy);
    /* Start on the rim — not in content */
    pick_screen_edge_target();
    g.x = g.tx;
    g.y = g.ty;
    clamp_pos();
    pick_next_behavior();

    g.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g.win), "shadow-cat");
    gtk_window_set_default_size(GTK_WINDOW(g.win), SIZE, SIZE);
    gtk_widget_set_size_request(g.win, SIZE, SIZE);
    gtk_window_set_decorated(GTK_WINDOW(g.win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(g.win), FALSE);
    g_signal_connect(g.win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(g.win, "realize", G_CALLBACK(on_realize), NULL);

    gtk_layer_init_for_window(GTK_WINDOW(g.win));
    gtk_layer_set_namespace(GTK_WINDOW(g.win), "shadow-cat");
    gtk_layer_set_layer(GTK_WINDOW(g.win), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_anchor(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(g.win), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(g.win), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
    apply_margins();

    g.da = gtk_drawing_area_new();
    gtk_widget_set_size_request(g.da, SIZE, SIZE);
    gtk_widget_add_events(g.da, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(g.da, "draw", G_CALLBACK(on_draw), NULL);
    g_signal_connect(g.da, "button-press-event", G_CALLBACK(on_button), NULL);
    gtk_container_add(GTK_CONTAINER(g.win), g.da);

    gtk_widget_show_all(g.win);
    g.tick_id = g_timeout_add(60, tick, NULL);
    gtk_main();
    if (g.ipc_fd >= 0)
        close(g.ipc_fd);
    return 0;
}
