/*
 * shadow-cat — orange-brained Hyprland desktop pet
 * One brain cell. Loud affection. Sudden zoomies. Procedural Cairo.
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
    B_FALL,          /* gravity fall — success / fail landing */
    B_SIDE_EYE,
    B_HISS,
    B_ACCEPT,
    B_COUNT
} Behavior;

/* Stunt style indices (jump / roll / scratch / fall) */
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
    SCR_STYLES = 4,

    FALL_GRACE = 0,  /* stuck landing, on feet */
    FALL_BELLY,      /* belly flop fail */
    FALL_TUMBLE,     /* spin fail then scramble */
    FALL_STYLES = 3,

    /* Left-click affection — 5 looks */
    PET_BOUNCE = 0,  /* happy bounce + hearts */
    PET_RUB,         /* weave / cheek-rub */
    PET_BELLY,       /* flop over, show belly */
    PET_NUZZLE,      /* lean head toward you */
    PET_LOAF,        /* melt into bliss loaf */
    PET_STYLES = 5
};

#define GRAVITY_PX 920.0   /* screen-down acceleration for air_z */

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
    int anim_style;      /* style variant for jump/roll/scratch/fall */
    double air_z;        /* visual jump height (px up) */
    double vz;           /* vertical air velocity (px/s), +up */
    int land_ok;         /* 1 = graceful landing, 0 = fail */
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
    double tail_t;       /* continuous fluid tail phase */
    double head_drop;    /* groom head lower */
    double pose_fat;     /* body roundness */
    double pose_long;    /* body length stretch */
    double pose_arch;    /* back arch */
    double pose_puff;    /* angry/scared fluff */
    double pose_head;    /* head scale */
    double gaze_x;       /* pupil look offset −1..1 (local +X = nose) */
    double gaze_y;       /* pupil look offset −1..1 (+down) */
    double pupil;        /* dilation 0..1 */
    double ear_l;        /* sleep ear twitch 0..1 (left) */
    double ear_r;        /* sleep ear twitch 0..1 (right) */
    double ear_wait;     /* seconds until next sleep twitch */
    double ear_phase;    /* >0 while a twitch is playing */
    int ear_which;       /* 0 left, 1 right, 2 both */
    guint tick_id;
    int mon_w, mon_h, mon_x, mon_y;
    int need_fast;
    int workspace_id;
    int obscured;        /* hidden while a fullscreen app is focused */
    double fade;         /* 0..1 draw opacity (soft reappear) */
    int pet_streak;      /* recent left-clicks; too many → overstim */
    double pet_age;      /* seconds since last pet */
    int overstim_left;   /* scratches remaining in overstim bout */
    double pet_sulk;     /* ignore affection while fleeing / cooling off */
    double nap_after;    /* >0: return to sleep after this many seconds */
    Behavior recent[4];  /* last distinct activities — anti-repeat */
    int nrecent;
    Motion last_motion;  /* avoid same gait twice in a row */
    WinRect wins[MAX_WINS];
    int nwins;
} Cat;

static Cat g;

static void set_beh(Behavior b, double dur);
static void pick_sleep_side(void);
static void pick_wander_target(void);
static void apply_margins(void);
static void update_gaze(double dt);
static void update_sleep_ears(double dt);
static double frand(void);
static void begin_fall(double height, double upward_v, int force_style);
static int free_tumble_draw(void);
static double gravity_pitch(void);

/* ---------- Hyprland IPC (cheap) ---------- */

/* Reject traversal / control chars in env-derived path pieces. */
static int env_token_ok(const char *s, int allow_slash)
{
    if (!s || !*s)
        return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 0x20 || *p == 0x7f)
            return 0;
        if (!allow_slash && *p == '/')
            return 0;
        if (*p == '\\')
            return 0;
    }
    if (strstr(s, ".."))
        return 0;
    return 1;
}

static int ipc_cmd_allowed(const char *cmd)
{
    static const char *const ok[] = {
        "cursorpos",
        "monitors",
        "j/activeworkspace",
        "j/clients",
        "j/activewindow",
    };
    if (!cmd)
        return 0;
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++) {
        if (strcmp(cmd, ok[i]) == 0)
            return 1;
    }
    return 0;
}

static int ipc_connect(void)
{
    const char *rt = getenv("XDG_RUNTIME_DIR");
    const char *sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!env_token_ok(rt, 1) || rt[0] != '/')
        return -1;
    if (!env_token_ok(sig, 0))
        return -1;

    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    int n = snprintf(path, sizeof(path), "%s/hypr/%s/.socket.sock", rt, sig);
    if (n < 0 || (size_t)n >= sizeof(path))
        return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, (size_t)n + 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int ipc_request(const char *cmd, char *out, size_t out_sz)
{
    if (!ipc_cmd_allowed(cmd) || !out || out_sz < 2)
        return -1;
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
        if (write(g.ipc_fd, cmd, len) != (ssize_t)len) {
            close(g.ipc_fd);
            g.ipc_fd = -1;
            return -1;
        }
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
        /* Clamp absurd IPC values so layout math stays sane */
        if (w < 320) w = 320;
        if (h < 240) h = 240;
        if (w > 16384) w = 16384;
        if (h > 16384) h = 16384;
        if (ox < -16384) ox = -16384;
        if (oy < -16384) oy = -16384;
        if (ox > 16384) ox = 16384;
        if (oy > 16384) oy = 16384;
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
    int mx = (int)lround(g.x) - g.mon_x;
    int my = (int)lround(g.y) - g.mon_y;
    static int last_mx = -99999, last_my = -99999;
    if (mx == last_mx && my == last_my)
        return;
    last_mx = mx;
    last_my = my;
    gtk_layer_set_margin(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_LEFT, mx);
    gtk_layer_set_margin(GTK_WINDOW(g.win), GTK_LAYER_SHELL_EDGE_TOP, my);
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

/* Feet stay screen-down on floors; pitch into climb; wall-cling on sides. */
static double vertical_travel(void)
{
    double spd = hypot(g.vx, g.vy);
    if (spd > 12.0)
        return fabs(g.vy) / spd;
    return fabs(sin(g.angle));
}

static int near_side_wall(void)
{
    double cx = cat_cx() - g.mon_x;
    return (cx < 130.0 || cx > g.mon_w - 130.0);
}

static double gravity_pitch(void)
{
    double climb = sin(g.angle); /* +down-screen, -up-screen */
    double v = vertical_travel();
    double pitch = climb * 0.20;
    double spd = hypot(g.vx, g.vy);

    /* Diagonal stride lean — tip into the path like Waycat run frames */
    if (spd > 18.0 && g.pose_walk > 0.12) {
        double travel = atan2(g.vy, g.vx);
        double diag = fabs(sin(2.0 * travel)); /* 1 at 45°, 0 on axes */
        pitch = climb * (0.10 + 0.14 * (1.0 - diag))
              + sin(travel) * (0.28 + 0.22 * fmin(1.0, spd / 160.0)) * diag;
        if (g.beh == B_DASH || g.motion == M_SPRINT || g.motion == M_DIAGONAL)
            pitch *= 1.15;
    }

    /* Stair lean — stronger when going mostly vertical off-wall */
    if (v > 0.45 && !near_side_wall())
        pitch = climb * (0.35 + 0.35 * v);
    if (g.air_z > 1.0 || g.beh == B_FALL)
        pitch += fmax(-0.35, fmin(0.35, -g.vz * 0.00055));
    if (pitch > 0.55) pitch = 0.55;
    if (pitch < -0.55) pitch = -0.55;
    return pitch;
}

/* Rotate feet toward the side wall while climbing it */
static double wall_cling_rot(void)
{
    double v = vertical_travel();
    if (v < 0.35 || !near_side_wall())
        return 0;
    int loco = (g.beh == B_WANDER || g.beh == B_DASH || g.beh == B_CORNER ||
                (g.beh == B_SCRATCH && g.anim_style == SCR_HUNT));
    if (!loco)
        return 0;
    double t = (v - 0.35) / 0.55;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    double amt = t * t * (3.0 - 2.0 * t);
    double left = (cat_cx() - g.mon_x < g.mon_w * 0.5);
    /* Body +Y is feet; swing feet into the wall */
    return (left ? 1.0 : -1.0) * (G_PI * 0.42) * amt;
}

/* Full body tumble allowed only for rolls / failed falls / twist jumps */
static int free_tumble_draw(void)
{
    if (g.beh == B_FLOP)
        return 1;
    if (g.beh == B_FALL && g.anim_style != FALL_GRACE)
        return 1;
    if (g.beh == B_JUMP && g.anim_style == JUMP_TWIST)
        return 1;
    return 0;
}

static void face_toward(double x, double y)
{
    set_angle_from_vec(x - cat_cx(), y - cat_cy());
}

static void pick_motion_for(Behavior b)
{
    /* Prefer natural diagonal walk / run gaits (neko-style) */
    static const Motion roam[] = {
        M_DIAGONAL, M_DIAGONAL, M_DIAGONAL, M_DIAGONAL,
        M_WALK, M_WALK, M_TROT, M_TROT, M_CREEP,
        M_ARC, M_SPRINT, M_ZIGZAG, M_HOP, M_PAUSING
    };
    static const Motion dash[] = {
        M_SPRINT, M_SPRINT, M_DIAGONAL, M_DIAGONAL, M_DIAGONAL,
        M_TROT, M_SKID, M_HOP, M_ZIGZAG
    };
    static const Motion corner[] = {
        M_DIAGONAL, M_DIAGONAL, M_WALK, M_WALK, M_CREEP, M_TROT, M_ARC, M_PAUSING
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
        if (g.motion == g.last_motion)
            g.motion = (g.motion == M_SPRINT) ? M_HOP : M_SPRINT;
        g.last_motion = g.motion;
        g.amp = 18 + frand() * 22;
        g.path_u = 0;
        g.ox = cat_cx();
        g.oy = cat_cy();
        g.spin_left = 0;
        g.dist0 = 0;
        return;
    }
    /* Prefer a gait different from the previous locomotion */
    Motion pick = tab[rand() % n];
    if (n > 1) {
        for (int tries = 0; tries < 6 && pick == g.last_motion; tries++)
            pick = tab[rand() % n];
    }
    g.motion = pick;
    g.last_motion = pick;
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
    /* Graceful baseline — dash/sprint still punchy */
    switch (g.motion) {
    case M_CREEP:    return 48.0;
    case M_WALK:     return 78.0;
    case M_TROT:     return 118.0;
    case M_SPRINT:   return 290.0;
    case M_DIAGONAL: return 102.0;
    case M_ZIGZAG:   return 85.0;
    case M_ARC:      return 82.0;
    case M_CIRCLE:   return 70.0;
    case M_BACKPEDAL:return 62.0;
    case M_SIDESTEP: return 68.0;
    case M_HOP:      return 120.0;
    case M_SKID:     return 200.0;
    case M_SPIN:     return 90.0;
    case M_FIGURE8:  return 78.0;
    case M_PAUSING:  return 72.0;
    default:         return 78.0;
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
    double turn = 5.2; /* rad/s — soft turns for grace */
    if (g.beh == B_DASH)
        turn = 8.5;

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
    double accel = (g.beh == B_DASH) ? 7.0 : 3.6;

    /* ease in/out along the trip — graceful slow-fast-slow */
    double progress = 1.0 - dist / g.dist0;
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;
    double ease = sin(progress * G_PI);
    double spd = max_spd * (0.58 + 0.42 * ease);
    /* longer soft coast into the target */
    if (dist < 160.0)
        spd *= fmax(0.22, dist / 160.0);
    if (dist < 40.0) {
        spd *= dist / 40.0;
        accel = fmin(accel, 2.2);
    }

    if (g.motion == M_PAUSING) {
        /* soft breath in motion instead of hard freeze */
        double pulse = 0.5 + 0.5 * sin(g.t * 5.0);
        spd *= 0.15 + 0.85 * pulse * pulse;
        accel = 3.5;
    } else if (g.motion == M_SIDESTEP) {
        /* crab-step but still nose toward travel — never reverse */
        smooth_turn_to(atan2(uy, ux), dt, turn);
        wish_x = ux * 0.55 + (-sin(g.angle)) * 0.45;
        wish_y = uy * 0.55 + cos(g.angle) * 0.45;
    } else if (g.motion == M_ZIGZAG) {
        double wiggle = sin(g.t * 5.5) * 0.55;
        wish_x = ux + px * wiggle;
        wish_y = uy + py * wiggle;
    } else if (g.motion == M_ARC) {
        double bend = sin(progress * G_PI) * 0.75 * ((g.amp > 25) ? 1.0 : -1.0);
        wish_x = ux + px * bend;
        wish_y = uy + py * bend;
    } else if (g.motion == M_DIAGONAL || g.motion == M_WALK
               || g.motion == M_TROT || g.motion == M_SPRINT) {
        /* Soft-commit onto 45° lanes — natural diagonal walk/run */
        double ax = fabs(ux), ay = fabs(uy);
        double ratio = fmin(ax, ay) / fmax(fmax(ax, ay), 1e-6);
        double a = atan2(uy, ux);
        double oct = G_PI / 4.0;
        int k = (int)lround(a / oct);
        /* Prefer odd octants (true diagonals) when path isn't flat */
        if (ratio > 0.18 && (k & 1) == 0) {
            double d1 = fabs(angle_diff(a, k * oct + oct));
            double d2 = fabs(angle_diff(a, k * oct - oct));
            k += (d1 <= d2) ? 1 : -1;
        }
        double snapped = k * oct;
        double blend = 0.0;
        if (g.motion == M_DIAGONAL)
            blend = 0.82;
        else if (ratio > 0.22)
            blend = 0.30 + 0.45 * fmin(1.0, ratio);
        if (g.beh == B_DASH)
            blend = fmax(blend, 0.55);
        if (blend > 0.01) {
            wish_x = cos(snapped) * blend + ux * (1.0 - blend);
            wish_y = sin(snapped) * blend + uy * (1.0 - blend);
        }
        if (g.motion == M_WALK) {
            accel = (g.beh == B_DASH) ? 6.0 : 3.4;
            turn = (g.beh == B_DASH) ? 7.0 : 4.6;
        } else if (g.motion == M_SPRINT) {
            accel = 9.2;
            turn = 9.0;
            spd *= 1.05;
        } else if (g.motion == M_TROT) {
            accel = 5.2;
            spd *= 0.92 + 0.08 * sin(g.t * 10.0);
            turn = 5.2;
        } else { /* DIAGONAL */
            accel = (g.beh == B_DASH) ? 7.5 : 4.0;
            turn = (g.beh == B_DASH) ? 7.5 : 5.0;
            /* steady diagonal cadence */
            spd *= 0.94 + 0.06 * sin(g.t * 8.0);
        }
    } else if (g.motion == M_HOP) {
        double hop = 0.4 + 0.6 * fabs(sin(g.t * 9.0));
        spd *= hop;
        accel = 10.0;
    } else if (g.motion == M_SKID) {
        accel = (dist < 100.0) ? 2.2 : 7.0;
        if (dist < 100.0)
            spd *= 0.5 + 0.5 * (dist / 100.0);
    } else if (g.motion == M_CREEP) {
        accel = 2.4;
        turn = 3.2;
        {
            double ax = fabs(ux), ay = fabs(uy);
            double ratio = fmin(ax, ay) / fmax(fmax(ax, ay), 1e-6);
            if (ratio > 0.22) {
                double a = atan2(uy, ux);
                double oct = G_PI / 4.0;
                int k = (int)lround(a / oct);
                if ((k & 1) == 0) {
                    double d1 = fabs(angle_diff(a, k * oct + oct));
                    double d2 = fabs(angle_diff(a, k * oct - oct));
                    k += (d1 <= d2) ? 1 : -1;
                }
                double snapped = k * oct;
                wish_x = cos(snapped) * 0.4 + ux * 0.6;
                wish_y = sin(snapped) * 0.4 + uy * 0.6;
            }
        }
    }

    /* Vertical: stair cadence or wall-climb reach rhythm */
    {
        double vdir = (hypot(wish_x, wish_y) > 1e-6)
                      ? fabs(wish_y) / hypot(wish_x, wish_y)
                      : vertical_travel();
        if (vdir > 0.45) {
            if (near_side_wall()) {
                /* cling: slower deliberate pulls */
                double pull = 0.55 + 0.45 * fabs(sin(g.frame * 3.6));
                spd *= 0.7 + 0.35 * pull;
                accel = fmin(accel, 5.5);
                turn = fmax(turn, 6.0);
            } else {
                /* stairs: pulse each step */
                double step = 0.35 + 0.65 * fmax(0.0, sin(g.frame * 5.5));
                spd *= 0.65 + 0.5 * step;
                accel = fmax(accel, 8.0);
            }
        }
    }

    steer(wish_x, wish_y, spd, dt, accel);

    /* Always face the way we move — no moonwalk / backpedal */
    if (hypot(g.vx, g.vy) > 12.0) {
        smooth_turn_to(atan2(g.vy, g.vx), dt, turn);
        if (fabs(g.vx) > 10.0)
            g.facing = (g.vx >= 0.0) ? 1.0 : -1.0;
    }

    apply_velocity(dt);

    /* Visual climb lift — stair hops / wall reaches (not real jump height) */
    if (g.beh != B_JUMP && g.beh != B_FALL) {
        double v = vertical_travel();
        if (v > 0.4 && hypot(g.vx, g.vy) > 20.0) {
            if (near_side_wall())
                g.air_z = 3.0 + fabs(sin(g.frame * 3.6)) * 7.0 * v;
            else
                g.air_z = fmax(0.0, sin(g.frame * 5.5)) * 9.0 * v;
        } else if (g.air_z > 0 && g.beh != B_SCRATCH) {
            g.air_z *= exp(-8.0 * dt);
            if (g.air_z < 0.4) g.air_z = 0;
        }
    }

    /* Slower leg cycle when strolling; livelier on dash */
    double gait_rate = (g.beh == B_DASH) ? 0.045 : 0.028;
    g.frame += dt * (3.2 + hypot(g.vx, g.vy) * gait_rate);
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
        if (ww > 16384 || wh > 16384) {
            p = end;
            continue;
        }
        if (wx < -16384 || wy < -16384 || wx > 16384 || wy > 16384) {
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

/* Classic desktop-pet diagonal: far point with real Δx and Δy */
static void pick_diagonal_roam_target(void)
{
    double cx = cat_cx(), cy = cat_cy();
    double m = 22.0;
    double min_x = g.mon_x + m;
    double max_x = g.mon_x + g.mon_w - SIZE - m;
    double min_y = g.mon_y + 48.0;
    double max_y = g.mon_y + g.mon_h - SIZE - m;
    int prefer_right = (cx < g.mon_x + g.mon_w * 0.5);
    int prefer_down = (cy < g.mon_y + g.mon_h * 0.5);
    if (frand() < 0.28)
        prefer_right = !prefer_right;
    if (frand() < 0.28)
        prefer_down = !prefer_down;

    double best_tx = cx, best_ty = cy, best_score = -1.0;
    for (int tries = 0; tries < 10; tries++) {
        double tx, ty;
        if (prefer_right)
            tx = g.mon_x + g.mon_w * (0.52 + frand() * 0.42) - SIZE * 0.5;
        else
            tx = g.mon_x + g.mon_w * (0.06 + frand() * 0.42) - SIZE * 0.5;
        if (prefer_down)
            ty = g.mon_y + g.mon_h * (0.52 + frand() * 0.40) - SIZE * 0.5;
        else
            ty = g.mon_y + g.mon_h * (0.10 + frand() * 0.40) - SIZE * 0.5;
        if (tx < min_x) tx = min_x;
        if (tx > max_x) tx = max_x;
        if (ty < min_y) ty = min_y;
        if (ty > max_y) ty = max_y;
        double dx = tx - cx, dy = ty - cy;
        double dist = hypot(dx, dy);
        if (dist < 140.0)
            continue;
        double ax = fabs(dx), ay = fabs(dy);
        double diag = fmin(ax, ay) / fmax(ax, ay); /* 1 = perfect 45° */
        double score = dist * (0.35 + 0.65 * diag);
        if (score > best_score) {
            best_score = score;
            best_tx = tx;
            best_ty = ty;
        }
    }
    if (best_score < 0.0) {
        /* fallback: push both axes from current spot */
        double span_x = (max_x - min_x) * (0.35 + frand() * 0.4);
        double span_y = (max_y - min_y) * (0.35 + frand() * 0.4);
        best_tx = prefer_right ? fmin(max_x, cx + span_x) : fmax(min_x, cx - span_x);
        best_ty = prefer_down ? fmin(max_y, cy + span_y) : fmax(min_y, cy - span_y);
    }
    g.tx = best_tx;
    g.ty = best_ty;
}

/* Sprint to the far side — away from the cursor / current spot */
static void pick_flee_target(void)
{
    double cx = cat_cx(), cy = cat_cy();
    double m = 10.0;
    double band = 36.0 + frand() * 50.0;
    /* Diagonal flee — away on both axes */
    int away_right = (g.cx <= cx);
    int away_down = (g.cy <= cy);
    if (away_right)
        g.tx = g.mon_x + g.mon_w - SIZE - m - frand() * band;
    else
        g.tx = g.mon_x + m + frand() * band;
    if (away_down)
        g.ty = g.mon_y + g.mon_h - SIZE - 40 - frand() * 100;
    else
        g.ty = g.mon_y + 50 + frand() * 100;
    /* Ensure the run isn't a flat horizontal dash */
    if (fabs(g.ty - cy) < 80.0)
        g.ty += away_down ? 120.0 : -120.0;
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

static void begin_overstim_flee(void)
{
    g.mood = fmax(-1.0, g.mood - 0.55);
    g.pet_streak = 0;
    g.overstim_left = 2 + (rand() % 2); /* 2–3 scratches, then dash */
    g.after_beh = -1;
    g.nap_after = 0; /* flee overrides nap timer; sleep after sulk settle */
    read_cursor(&g.cx, &g.cy);
    set_beh(B_SCRATCH, 0.85 + frand() * 0.35);
    g.anim_style = (frand() < 0.7) ? SCR_FURY : SCR_DIG;
}

/* Long side nap — default life mode */
static void go_to_sleep(double nap_dur)
{
    if (nap_dur < 8.0)
        nap_dur = 20.0 + frand() * 40.0;
    g.nap_after = 0;
    g.after_beh = -1;
    pick_sleep_side();
    {
        double cx = cat_cx() - g.mon_x;
        int near = (cx < 160.0 || cx > g.mon_w - 160.0);
        double dy = fabs(g.ty - g.y);
        double dx = fabs(g.tx - g.x);
        if (near && dx < 80.0 && dy < 120.0) {
            g.x = g.tx;
            g.y = g.ty;
            apply_margins();
            set_beh(B_SLEEP, nap_dur);
        } else {
            g.after_beh = B_SLEEP;
            g.after_dur = nap_dur;
            set_beh(B_CORNER, 10.0);
        }
    }
}

/* Interaction wake: do something, then nap again in 3–10s */
static void wake_then_nap(Behavior act, double act_dur)
{
    g.nap_after = 3.0 + frand() * 7.0;
    g.after_beh = -1;
    if (act == B_WANDER || act == B_DASH) {
        pick_wander_target();
        set_beh(act, act_dur);
    } else if (act == B_ACCEPT) {
        read_cursor(&g.cx, &g.cy);
        set_beh(B_ACCEPT, act_dur);
    } else if (act == B_SCRATCH || act == B_STARE || act == B_SIDE_EYE) {
        read_cursor(&g.cx, &g.cy);
        set_beh(act, act_dur);
    } else {
        set_beh(act, act_dur);
    }
}

static void wake_random_activity(void)
{
    /* Orange-cat stir menu: zoomies, vacant stares, chaos flop */
    Behavior opts[] = {
        B_WANDER, B_WANDER, B_WANDER, B_DASH,
        B_LOOK, B_LOOK, B_STARE, B_SIDE_EYE,
        B_ACCEPT, B_ACCEPT, B_FLOP, B_JUMP,
        B_TAILFLICK, B_STRETCH, B_LOAF, B_GROOM
    };
    int n = (int)(sizeof(opts) / sizeof(opts[0]));
    Behavior act = opts[rand() % n];
    double dur = 1.2 + frand() * 2.2;
    if (act == B_WANDER)
        dur = 2.8 + frand() * 3.2;
    if (act == B_DASH)
        dur = 1.4 + frand() * 1.2;
    if (act == B_ACCEPT)
        dur = 1.6 + frand() * 1.0;
    if (act == B_LOOK || act == B_STARE)
        dur = 1.5 + frand() * 2.0; /* vacant stare lasts */
    if (act == B_FLOP || act == B_JUMP)
        dur = 1.0 + frand() * 0.8;
    wake_then_nap(act, dur);
    if ((act == B_WANDER || act == B_DASH) &&
        (g.motion == M_SIDESTEP || g.motion == M_CIRCLE
         || g.motion == M_FIGURE8 || g.motion == M_SPIN))
        g.motion = M_DIAGONAL;
}

static void continue_overstim_or_flee(void)
{
    if (g.overstim_left > 1) {
        g.overstim_left--;
        read_cursor(&g.cx, &g.cy);
        set_beh(B_SCRATCH, 0.75 + frand() * 0.4);
        g.anim_style = (frand() < 0.55) ? SCR_FURY : ((frand() < 0.5) ? SCR_DIG : SCR_POST);
        return;
    }
    g.overstim_left = 0;
    pick_flee_target();
    /* Forgets fast — orange brain cell already moved on */
    g.pet_sulk = 2.2 + frand() * 2.0;
    set_beh(B_DASH, 1.8 + frand() * 1.1);
    g.motion = M_DIAGONAL;
    {
        double dx = g.tx - cat_cx();
        double dy = g.ty - cat_cy();
        double len = hypot(dx, dy);
        if (len < 1.0) len = 1.0;
        g.vx = (dx / len) * 280.0;
        g.vy = (dy / len) * 280.0;
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

    /* Prefer natural diagonal roam across the desktop */
    if (roll < 0.72) {
        pick_diagonal_roam_target();
    } else if (g.nwins > 0 && roll < 0.78) {
        pick_content_target(&g.wins[rand() % g.nwins]);
    } else if (g.nwins > 0 && roll < 0.90) {
        pick_window_edge_target(&g.wins[rand() % g.nwins]);
    } else {
        pick_screen_edge_target();
    }

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

    /* If the path is nearly axis-aligned, nudge onto a diagonal */
    {
        double dx = g.tx - cat_cx();
        double dy = g.ty - cat_cy();
        double ax = fabs(dx), ay = fabs(dy);
        double dist = hypot(dx, dy);
        if (dist > 80.0 && fmin(ax, ay) / fmax(ax, ay) < 0.28) {
            double nudge = dist * (0.35 + frand() * 0.25);
            if (ay < ax)
                g.ty += (dy >= 0 ? 1.0 : -1.0) * nudge;
            else
                g.tx += (dx >= 0 ? 1.0 : -1.0) * nudge;
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
    return b == B_JUMP || b == B_FLOP || b == B_SCRATCH || b == B_FALL;
}

static void pick_stunt_style(Behavior b)
{
    if (b == B_JUMP)
        g.anim_style = rand() % JUMP_STYLES;
    else if (b == B_FLOP)
        g.anim_style = rand() % ROLL_STYLES;
    else if (b == B_SCRATCH)
        g.anim_style = rand() % SCR_STYLES;
    else if (b == B_FALL) {
        /* ~55% stuck landings, rest fail */
        double r = frand();
        if (r < 0.55)
            g.anim_style = FALL_GRACE;
        else if (r < 0.80)
            g.anim_style = FALL_BELLY;
        else
            g.anim_style = FALL_TUMBLE;
        g.land_ok = (g.anim_style == FALL_GRACE);
    } else
        g.anim_style = 0;
    if (b != B_FALL) {
        g.air_z = 0;
        g.vz = 0;
    }
    g.twist = 0;
    g.path_u = 0;
    g.ox = cat_cx();
    g.oy = cat_cy();
}

/* Start a gravity fall from current air height (or given height). */
static void begin_fall(double height, double upward_v, int force_style)
{
    if (height < 18.0)
        height = 18.0 + frand() * 40.0;
    g.after_beh = -1;
    set_beh(B_FALL, 2.4 + frand() * 1.2);
    g.air_z = height;
    g.vz = upward_v;
    g.path_u = 0;
    if (force_style >= 0) {
        g.anim_style = force_style % FALL_STYLES;
        g.land_ok = (g.anim_style == FALL_GRACE);
    }
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
    /* Stable default silhouette — idle must not thrash fat/long/scale. */
    double tsit = 0, troll = 0, tsx = 1.0, tsy = 1.0;
    double twalk = 0, tsleep = 0, tgroom = 0, thiss = 0, tpet = 0, tnarrow = 0;
    double tbob = 0, twag = 0.12, thead = 0;
    /* Default: a little skinny — lean body, modest length */
    double tfat = 0.10, tlong = 0.58, tarch = 0.10, tpuff = 0.0, ths = 1.0;
    double spd = hypot(g.vx, g.vy);
    double move = fmin(1.0, spd / 130.0); /* 0 idle … 1 full locomotion */
    double air = fmin(1.0, g.air_z / 40.0);

    switch (g.beh) {
    case B_SLEEP:
        /* curled nap — round loaf curl */
        tsit = 4.5; tsleep = 1.0; twag = 0.28; tbob = 0;
        tfat = 0.22; tlong = 0.36; tarch = -0.10; ths = 0.96;
        tsx = 1.14; tsy = 0.76;
        break;

    case B_LOAF:
        /* classic cat loaf — compact, tucked paws vibe */
        tsit = 3.0; twag = 0.42; tbob = 0.15;
        tfat = 0.20; tlong = 0.42; tarch = -0.04;
        tsx = 1.08; tsy = 0.84; ths = 1.02;
        break;

    case B_LOOK:
        /* glance: eyes/head vibe only — keep body */
        tsit = 1.2; twag = 0.55; tbob = 0.35; ths = 1.04;
        break;

    case B_SIDE_EYE:
    case B_STARE:
        tsit = 1.6; tnarrow = 1.0; twag = 0.48; tbob = 0.15; ths = 1.03;
        tarch = 0.08;
        break;

    case B_TAILFLICK:
        /* annoyance is in the tail, not a new silhouette */
        tsit = 1.0; twag = 1.1; tbob = 0.6; tarch = 0.2;
        break;

    case B_GROOM:
        tgroom = 1.0; thead = 1.0; tsit = 1.8; twag = 0.5;
        tarch = 0.22; ths = 1.02;
        break;

    case B_STRETCH: {
        /* timed stretch cycle: reach → hold → ease back */
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 1.0;
        if (u > 1) u = 1;
        double reach = (u < 0.35) ? smootherstep(u / 0.35)
                     : (u < 0.65) ? 1.0
                     : (1.0 - smootherstep((u - 0.65) / 0.35));
        tsit = 0.4 + 0.6 * (1.0 - reach);
        tsx = 1.0 + 0.55 * reach;
        tsy = 1.0 - 0.42 * reach;
        tlong = 0.45 + 0.5 * reach;
        tfat = 0.16 - 0.06 * reach;
        tarch = -0.12 * reach;
        ths = 1.0 - 0.12 * reach;
        thead = 0.25 * reach;
        twag = 0.2 + 0.15 * reach;
        break;
    }

    case B_ACCEPT: {
        /* Affection silhouette from the chosen pet style */
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 0;
        if (u > 1) u = 1;
        tpet = 1.0;
        twag = 0.7;
        ths = 1.08;
        if (g.anim_style == PET_BOUNCE) {
            tbob = 3.2; tsit = 1.2; twag = 0.9; tfat = 0.22;
            tsx = 1.0 + 0.08 * sin(g.t * 14.0);
            tsy = 1.0 - 0.08 * sin(g.t * 14.0);
        } else if (g.anim_style == PET_RUB) {
            tbob = 1.4; tsit = 1.0; twalk = 0.35; twag = 0.85;
            tlong = 0.55; tarch = 0.15;
        } else if (g.anim_style == PET_BELLY) {
            troll = 0.95 * smootherstep(fmin(1.0, u / 0.25));
            if (u > 0.75)
                troll *= 1.0 - smootherstep((u - 0.75) / 0.25);
            tsit = 2.5; tsx = 1.2; tsy = 0.75; tfat = 0.28;
            twag = 0.55; tbob = 0.8; ths = 1.1;
        } else if (g.anim_style == PET_NUZZLE) {
            tsit = 1.6; thead = 0.35; tbob = 1.0; twag = 0.6;
            tarch = 0.2; ths = 1.12; tfat = 0.2;
        } else { /* PET_LOAF */
            tsit = 3.2; tbob = 0.35; twag = 0.25; tfat = 0.24;
            tsx = 1.1; tsy = 0.82; ths = 1.02; tlong = 0.4;
        }
        break;
    }

    case B_HISS:
        /* arched scare puff — emotion shape */
        thiss = 1.0; tnarrow = 1.0; twag = 0.95; tbob = 0.7;
        tarch = 0.9; tpuff = 0.85; ths = 1.12;
        tsx = 0.88; tsy = 1.18; tlong = 0.5; tfat = 0.22;
        break;

    case B_WANDER:
    case B_DASH:
    case B_CORNER: {
        /* Shape follows gait intensity — idle coast returns to baseline body */
        double gait_sx = 1.0, gait_sy = 1.0, gait_long = 0.60, gait_fat = 0.10;
        double gait_arch = 0.12, gait_bob = 1.6, gait_wag = 0.4;
        if (g.motion == M_CREEP) {
            gait_sx = 1.12; gait_sy = 0.86; gait_long = 0.74; gait_fat = 0.14;
            gait_arch = 0.35; gait_bob = 0.7; gait_wag = 0.25;
        } else if (g.motion == M_SPRINT || g.beh == B_DASH) {
            gait_sx = 1.34; gait_sy = 0.72; gait_long = 0.92; gait_fat = 0.06;
            gait_arch = 0.18; gait_bob = 2.4; gait_wag = 0.65;
        } else if (g.motion == M_HOP) {
            gait_sx = 0.94; gait_sy = 1.1; gait_long = 0.55; gait_bob = 5.0;
            gait_fat = 0.10; gait_arch = 0.05;
        } else if (g.motion == M_SKID) {
            gait_sx = 1.34; gait_sy = 0.7; gait_long = 0.84; gait_arch = -0.05;
            gait_bob = 1.0; gait_fat = 0.08;
        } else if (g.motion == M_TROT) {
            gait_sx = 1.14; gait_sy = 0.88; gait_long = 0.66; gait_bob = 2.8;
            gait_fat = 0.10; gait_arch = 0.14;
        } else if (g.motion == M_DIAGONAL) {
            gait_sx = 1.18; gait_sy = 0.86; gait_long = 0.72; gait_fat = 0.08;
            gait_arch = 0.16; gait_bob = 2.2; gait_wag = 0.5;
        } else {
            /* walk / arc / etc — skinny standing proportions */
            gait_sx = 1.10; gait_sy = 0.90; gait_long = 0.64; gait_fat = 0.10;
            gait_arch = 0.14; gait_bob = 1.8;
        }
        tsx = 1.0 + (gait_sx - 1.0) * move;
        tsy = 1.0 + (gait_sy - 1.0) * move;
        tlong = 0.58 + (gait_long - 0.58) * move;
        tfat = 0.10 + (gait_fat - 0.10) * move;
        tarch = 0.08 + (gait_arch - 0.08) * move;
        tbob = gait_bob * move;
        twag = 0.12 + (gait_wag - 0.12) * fmax(move, 0.15);
        twalk = move;

        /* Vertical travel → wall-climb or stair-step silhouette */
        {
            double v = vertical_travel() * move;
            if (v > 0.35) {
                if (near_side_wall()) {
                    /* cling: long body, arched, stretched toward climb */
                    tlong = fmax(tlong, 0.55 + 0.45 * v);
                    tarch = fmax(tarch, 0.35 + 0.4 * v);
                    tsx = 0.85 + 0.1 * (1.0 - v);
                    tsy = 1.15 + 0.25 * v;
                    tbob = fmax(tbob, 2.5 * v);
                    twalk = fmax(twalk, 0.7 * v);
                } else {
                    /* stairs: compact steps, punchy bob */
                    tarch = fmax(tarch, 0.25 * v);
                    tsx = 1.0 + 0.12 * v;
                    tsy = 1.0 - 0.08 * v;
                    tbob = fmax(tbob, 4.5 * v);
                    twalk = fmax(twalk, 0.85 * v);
                    tlong = fmax(tlong, 0.5);
                }
            }
        }
        /* tiny settle-sit when nearly stopped while still in wander */
        if (move < 0.2)
            tsit = 1.2 * (1.0 - move / 0.2);
        break;
    }

    case B_JUMP: {
        /* squash/stretch from air height + vertical speed, not style roulette */
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 0;
        if (u > 1) u = 1;
        twalk = 0.25; tbob = 0;
        if (u < 0.12) {
            /* crouch takeoff */
            tsx = 1.2; tsy = 0.72; tarch = 0.35; tlong = 0.4;
        } else if (air > 0.08 && u < 0.88) {
            /* airborne: lengthen on leap, gather on pop/twist */
            if (g.anim_style == JUMP_LEAP) {
                tsx = 1.35; tsy = 0.7; tlong = 0.85; tarch = 0.1;
            } else if (g.anim_style == JUMP_TWIST) {
                tsx = 0.92; tsy = 1.12; tarch = 0.4; ths = 1.08;
            } else {
                tsx = 0.9 + 0.15 * air; tsy = 1.15 - 0.15 * air;
                tarch = 0.35; tlong = 0.4;
            }
        } else {
            /* land compress */
            tsx = 1.28; tsy = 0.68; tfat = 0.22; tarch = 0.15;
        }
        break;
    }

    case B_FALL: {
        twalk = 0.15; tbob = 0; twag = 0.3;
        if (g.air_z > 2.0) {
            if (g.anim_style == FALL_GRACE) {
                tsx = 0.9; tsy = 1.15; tarch = 0.3; tlong = 0.42;
            } else if (g.anim_style == FALL_BELLY) {
                tsx = 1.35; tsy = 0.62; tfat = 0.3; tlong = 0.65; tpuff = 0.2;
            } else {
                tsx = 0.95; tsy = 1.1; tpuff = 0.4; tarch = 0.4;
            }
        } else if (g.land_ok) {
            tsx = 1.22; tsy = 0.74; tsit = 1.4; tfat = 0.2;
        } else if (g.anim_style == FALL_BELLY) {
            tsx = 1.4; tsy = 0.55; tfat = 0.35; tsleep = 0.3; tsit = 2.8;
        } else {
            tsx = 1.1; tsy = 0.85; tpuff = 0.5; twalk = 0.35;
        }
        break;
    }

    case B_FLOP: {
        /* roll activity — tip/roll from progress, keep skinny soft body */
        double u = (g.dur > 0.01) ? (g.t / g.dur) : 0;
        if (u > 1) u = 1;
        tsit = 1.5; twag = 0.3; tbob = 0.4; twalk = 0.1;
        if (g.anim_style == ROLL_SIDE) {
            troll = sin(u * G_PI * 2.0) * 1.05;
            tsx = 1.08; tsy = 0.85;
        } else if (g.anim_style == ROLL_SOMERSAULT) {
            tarch = 0.3; tlong = 0.55;
        } else if (g.anim_style == ROLL_BARREL) {
            tpuff = 0.2; ths = 1.05;
        } else { /* LOG */
            troll = sin(u * G_PI) * 0.85;
            tsx = 1.2; tsy = 0.7; tlong = 0.6;
        }
        break;
    }

    case B_SCRATCH: {
        twag = 0.55; tbob = 1.2; twalk = 0.25; tpuff = 0.25; ths = 1.06;
        if (g.anim_style == SCR_HUNT) {
            twalk = fmax(0.35, move); tlong = 0.55 + 0.2 * move;
            tarch = 0.4; tsx = 1.0 + 0.15 * move; tsy = 1.0 - 0.12 * move;
        } else if (g.anim_style == SCR_POST) {
            /* reared rake */
            tsx = 0.78; tsy = 1.4; tarch = 0.65; tlong = 0.35; tsit = -1.5;
        } else if (g.anim_style == SCR_DIG) {
            tsx = 1.12; tsy = 0.78; tarch = 0.7; thead = 0.65; tsit = 1.0;
        } else { /* FURY */
            tpuff = 0.75; tarch = 0.45; twag = 0.95; tbob = 2.4;
        }
        break;
    }

    default:
        break;
    }

    double b = smootherstep(g.blend_t / fmax(g.blend_dur, 0.05));
    if (!is_locomotion(g.beh) && is_locomotion(g.prev_beh) && b < 1.0) {
        /* coast-out legs only — do not keep stretched sprint body */
        twalk = fmax(twalk, (1.0 - b) * fmin(1.0, spd / 80.0));
        tbob = fmax(tbob, (1.0 - b) * 1.2 * fmin(1.0, spd / 80.0));
    }
    if (g.beh == B_SLEEP && b < 1.0) {
        tsit *= b;
        tsleep *= b;
        tsit += (1.0 - b) * g.pose_sit * 0.25;
    }
    if (g.prev_beh == B_SLEEP && g.beh != B_SLEEP && b < 1.0) {
        tsleep = fmax(tsleep, 1.0 - b);
        tsit = fmax(tsit, 3.5 * (1.0 - b));
    }

    /* Structural body morphs ease slower; activity cues snappier */
    double tau_body = 0.28;
    double tau_act = 0.14;
    if (g.beh == B_SLEEP || g.prev_beh == B_SLEEP)
        tau_body = 0.4;
    if (g.beh == B_HISS || g.beh == B_ACCEPT || g.beh == B_STRETCH || g.beh == B_JUMP || g.beh == B_FALL)
        tau_body = 0.12;

    pose_approach(&g.pose_sit, tsit, dt, tau_body);
    pose_approach(&g.pose_roll, troll, dt, tau_act);
    pose_approach(&g.pose_sx, tsx, dt, tau_body);
    pose_approach(&g.pose_sy, tsy, dt, tau_body);
    pose_approach(&g.pose_walk, twalk, dt, 0.12);
    pose_approach(&g.pose_sleep, tsleep, dt, tau_body);
    pose_approach(&g.pose_groom, tgroom, dt, tau_act);
    pose_approach(&g.pose_hiss, thiss, dt, 0.1);
    pose_approach(&g.pose_pet, tpet, dt, 0.12);
    pose_approach(&g.pose_narrow, tnarrow, dt, 0.14);
    pose_approach(&g.pose_bob, tbob, dt, tau_act);
    pose_approach(&g.pose_wag, twag, dt, 0.12);
    pose_approach(&g.head_drop, thead, dt, tau_act);
    pose_approach(&g.pose_fat, tfat, dt, tau_body);
    pose_approach(&g.pose_long, tlong, dt, tau_body);
    pose_approach(&g.pose_arch, tarch, dt, tau_body);
    pose_approach(&g.pose_puff, tpuff, dt, 0.12);
    pose_approach(&g.pose_head, ths, dt, tau_body);
    g.blend_t += dt;
    update_gaze(dt);
    update_sleep_ears(dt);
}

/* Sparse sleep ear flicks — long random gaps, small motion */
static void update_sleep_ears(double dt)
{
    if (g.beh != B_SLEEP && g.pose_sleep < 0.45) {
        pose_approach(&g.ear_l, 0.0, dt, 0.18);
        pose_approach(&g.ear_r, 0.0, dt, 0.18);
        g.ear_phase = 0.0;
        return;
    }

    if (g.ear_phase > 0.0) {
        /* ~0.4s soft flick up then settle */
        double dur = 0.42;
        g.ear_phase += dt / dur;
        double amp = 0.0;
        if (g.ear_phase < 1.0) {
            double u = g.ear_phase;
            /* quick peak, gentle settle — not a big alert */
            amp = sin(u * G_PI) * (0.28 + 0.18 * sin(u * G_PI * 2.0) * 0.35);
            if (amp < 0.0) amp = 0.0;
        } else {
            g.ear_phase = 0.0;
            /* Long quiet stretches; sometimes even longer */
            if (frand() < 0.35)
                g.ear_wait = 14.0 + frand() * 18.0; /* 14–32s */
            else
                g.ear_wait = 6.0 + frand() * 10.0;  /* 6–16s */
            amp = 0.0;
        }
        double tl = (g.ear_which != 1) ? amp : 0.0;
        double tr = (g.ear_which != 0) ? amp : 0.0;
        /* Rare tiny echo on the other ear */
        if (g.ear_which == 0 && g.ear_phase > 0.35 && g.ear_phase < 0.75)
            tr = amp * 0.15;
        if (g.ear_which == 1 && g.ear_phase > 0.35 && g.ear_phase < 0.75)
            tl = amp * 0.15;
        pose_approach(&g.ear_l, tl, dt, 0.05);
        pose_approach(&g.ear_r, tr, dt, 0.05);
        return;
    }

    pose_approach(&g.ear_l, 0.0, dt, 0.16);
    pose_approach(&g.ear_r, 0.0, dt, 0.16);
    g.ear_wait -= dt;
    if (g.ear_wait <= 0.0) {
        g.ear_phase = 0.001;
        double r = frand();
        if (r < 0.48)
            g.ear_which = 0;
        else if (r < 0.92)
            g.ear_which = 1;
        else
            g.ear_which = 2; /* rare both */
    }
}

static void update_gaze(double dt)
{
    double tx = 0.0, ty = 0.0, tdil = 0.55;
    double dx = g.cx - cat_cx();
    double dy = g.cy - cat_cy();
    double dist = hypot(dx, dy);
    /* Local space: +X toward nose (after facing flip) */
    double lx = (dist > 1e-3) ? (dx * g.facing) / fmax(SIZE * 1.1, dist * 0.35) : 0.0;
    double ly = (dist > 1e-3) ? dy / fmax(SIZE * 1.1, dist * 0.35) : 0.0;
    if (lx > 1.0) lx = 1.0;
    if (lx < -1.0) lx = -1.0;
    if (ly > 1.0) ly = 1.0;
    if (ly < -1.0) ly = -1.0;
    double spd = hypot(g.vx, g.vy);

    switch (g.beh) {
    case B_SLEEP:
        tx = 0.0;
        ty = 0.35;
        tdil = 0.12;
        break;
    case B_LOOK:
        /* vacant one-brain-cell pan */
        tx = sin(g.t * 1.1) * 0.95;
        ty = sin(g.t * 0.7 + 1.3) * 0.55;
        tdil = 0.78;
        break;
    case B_STARE:
        /* locks on… forgets… locks on */
        if (fmod(g.t, 2.4) < 1.6) {
            tx = lx * 0.95;
            ty = ly * 0.85;
            tdil = 0.85;
        } else {
            tx = sin(g.t * 4.0) * 0.3;
            ty = 0.2;
            tdil = 0.95; /* huge vacant pupils */
        }
        break;
    case B_SIDE_EYE:
        tx = (lx >= 0 ? 0.75 : -0.75) + lx * 0.15;
        ty = ly * 0.35 + 0.15;
        tdil = 0.42;
        break;
    case B_ACCEPT:
        tx = lx * 0.85;
        ty = ly * 0.5 + 0.1;
        tdil = 0.95; /* heart-eyes dilation */
        break;
    case B_SCRATCH:
        tx = lx * 1.0;
        ty = ly * 0.9 - 0.1;
        tdil = 0.35;
        break;
    case B_HISS:
        tx = lx * 0.9;
        ty = ly * 0.5;
        tdil = 0.18;
        break;
    case B_GROOM:
        tx = -0.45 + 0.1 * sin(g.t * 6.0);
        ty = 0.7;
        tdil = 0.4;
        break;
    case B_STRETCH:
        tx = 0.55;
        ty = -0.25 + 0.1 * sin(g.t * 2.0);
        tdil = 0.58;
        break;
    case B_WANDER:
    case B_DASH:
    case B_CORNER:
        /* Look ahead (nose = +X); glance vertically with climb */
        tx = 0.55 + 0.2 * sin(g.t * 4.0);
        ty = fmax(-0.45, fmin(0.45, g.vy / 100.0)) + 0.08 * sin(g.t * 2.5);
        tdil = 0.48 + 0.22 * fmin(1.0, spd / 160.0);
        if (g.beh == B_DASH)
            tdil = fmin(0.85, tdil + 0.15);
        break;
    case B_JUMP:
    case B_FALL:
        tx = 0.15;
        ty = (g.vz > 0) ? -0.55 : 0.65;
        tdil = 0.7;
        break;
    case B_FLOP:
        tx = sin(g.t * 3.0) * 0.4;
        ty = 0.5;
        tdil = 0.45;
        break;
    case B_TAILFLICK:
        tx = sin(g.t * 5.0) * 0.5;
        ty = -0.1;
        tdil = 0.55;
        break;
    case B_LOAF:
    default:
        tx = sin(g.t * 0.65) * 0.3;
        ty = sin(g.t * 0.5 + 1.2) * 0.18;
        tdil = 0.5;
        if (dist < SIZE * 2.8) {
            tx = lx * 0.7;
            ty = ly * 0.55;
            tdil = 0.68;
        }
        break;
    }

    if (g.pose_hiss > 0.4)
        tdil = fmin(tdil, 0.22);
    if (g.pose_pet > 0.3)
        tdil = fmax(tdil, 0.75);
    if (g.mood < -0.3)
        tdil *= 0.85;

    pose_approach(&g.gaze_x, tx, dt, 0.1);
    pose_approach(&g.gaze_y, ty, dt, 0.1);
    pose_approach(&g.pupil, tdil, dt, 0.12);
}

static void remember_beh(Behavior b)
{
    if (g.nrecent > 0 && g.recent[0] == b)
        return;
    for (int i = 3; i > 0; i--)
        g.recent[i] = g.recent[i - 1];
    g.recent[0] = b;
    if (g.nrecent < 4)
        g.nrecent++;
}

static int recently_did(Behavior b)
{
    int n = g.nrecent < 3 ? g.nrecent : 3;
    for (int i = 0; i < n; i++) {
        if (g.recent[i] == b)
            return 1;
    }
    return b == g.beh;
}

/* LOOK / STARE / SIDE_EYE all feel like “looking” */
static int look_family(Behavior b)
{
    return b == B_LOOK || b == B_STARE || b == B_SIDE_EYE;
}

static int recently_look_family(void)
{
    if (look_family(g.beh))
        return 1;
    int n = g.nrecent < 3 ? g.nrecent : 3;
    for (int i = 0; i < n; i++) {
        if (look_family(g.recent[i]))
            return 1;
    }
    return 0;
}

static void set_beh(Behavior b, double dur)
{
    if (b != g.beh) {
        remember_beh(g.beh);
        g.prev_beh = g.beh;
        g.blend_t = 0;
        g.blend_dur = 0.4;
        if (g.prev_beh == B_SLEEP || b == B_SLEEP)
            g.blend_dur = 0.85;
        else if (is_locomotion(g.prev_beh) && !is_locomotion(b))
            g.blend_dur = 0.55; /* coast to a stop */
        else if (!is_locomotion(g.prev_beh) && is_locomotion(b))
            g.blend_dur = 0.4;  /* wind up */
        else if (g.prev_beh == B_HISS || b == B_HISS || b == B_ACCEPT)
            g.blend_dur = 0.22;
        else if (b == B_STRETCH || b == B_JUMP || b == B_FALL || b == B_FLOP)
            g.blend_dur = 0.2;
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
    } else if (b == B_ACCEPT) {
        int style = rand() % PET_STYLES;
        /* Don't reuse the same pet animation back-to-back */
        if (g.prev_beh == B_ACCEPT && style == g.anim_style)
            style = (style + 1 + rand() % (PET_STYLES - 1)) % PET_STYLES;
        g.anim_style = style;
        g.twist = 0;
        g.path_u = 0;
        g.ox = cat_cx();
        g.oy = cat_cy();
        read_cursor(&g.cx, &g.cy);
        /* style-tuned durations */
        if (g.anim_style == PET_BOUNCE)
            g.dur = 1.1 + frand() * 0.5;
        else if (g.anim_style == PET_RUB)
            g.dur = 1.4 + frand() * 0.6;
        else if (g.anim_style == PET_BELLY)
            g.dur = 1.6 + frand() * 0.5;
        else if (g.anim_style == PET_NUZZLE)
            g.dur = 1.2 + frand() * 0.5;
        else
            g.dur = 1.8 + frand() * 0.7;
        if (dur > 0.1)
            g.dur = fmax(dur, g.dur * 0.85);
        g.vx *= 0.4;
        g.vy *= 0.4;
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

/* Sleeps a lot, but when the one brain cell fires… chaos. */
static void pick_next_behavior(void)
{
    g.after_beh = -1;

    /* Mood drifts sunny / chaotic, not mean */
    g.mood += (frand() - 0.35) * 0.28;
    if (g.mood > 1) g.mood = 1;
    if (g.mood < -1) g.mood = -1;

    struct { Behavior b; int w; double dmin, dmax; } opts[] = {
        { B_SLEEP,     58, 20.0, 55.0 },
        { B_LOAF,       6, 2.0, 4.5 },
        { B_WANDER,     9, 2.5, 5.5 },
        { B_LOOK,       6, 1.4, 3.0 },
        { B_STARE,      3, 1.5, 3.5 },
        { B_STRETCH,    4, 1.2, 2.2 },
        { B_TAILFLICK,  4, 0.8, 1.8 },
        { B_FLOP,       3, 1.0, 1.8 },
        { B_JUMP,       2, 0.8, 1.4 },
        { B_DASH,       2, 1.2, 2.2 },
        { B_GROOM,      2, 1.5, 3.0 },
        { B_SIDE_EYE,   2, 1.0, 2.5 },
    };
    int n = (int)(sizeof(opts) / sizeof(opts[0]));
    int total = 0;
    for (int i = 0; i < n; i++) {
        int w = opts[i].w;
        Behavior b = opts[i].b;
        if (b == g.beh)
            w = (b == B_SLEEP) ? 32 : 0;
        else if (b != B_SLEEP && recently_did(b))
            w = 0;
        if (look_family(b) && recently_look_family())
            w = (w > 1) ? 1 : 0; /* still allows vacant stares sometimes */
        if (w < 0) w = 0;
        opts[i].w = w;
        total += w;
    }
    if (total < 1) {
        go_to_sleep(25.0 + frand() * 35.0);
        return;
    }
    int r = rand() % total;
    Behavior pick = B_SLEEP;
    double dmin = 25, dmax = 55;
    for (int i = 0; i < n; i++) {
        if (opts[i].w <= 0)
            continue;
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
        go_to_sleep(dur);
        return;
    }

    /* Brief stir — always schedule a return nap */
    g.nap_after = 3.0 + frand() * 7.0;
    if (pick == B_WANDER || pick == B_DASH) {
        pick_wander_target();
        set_beh(pick, dur);
        if (pick == B_DASH)
            g.motion = (frand() < 0.6) ? M_SPRINT : M_DIAGONAL;
        return;
    }
    set_beh(pick, dur);
}

/* ---------- Drawing (classic orange / ginger cat) ---------- */

/* Ginger #E8913A · sleepy #C48A58 · eye punch #2A1810 */
#define FUR_R 0.910
#define FUR_G 0.569
#define FUR_B 0.227
#define MUTE_R 0.769
#define MUTE_G 0.541
#define MUTE_B 0.345
#define INK_R 0.165
#define INK_G 0.094
#define INK_B 0.063

static void fur_color(double *r, double *gg, double *b)
{
    double s = fmax(0.0, fmin(1.0, g.pose_sleep));
    *r = FUR_R * (1.0 - s) + MUTE_R * s;
    *gg = FUR_G * (1.0 - s) + MUTE_G * s;
    *b = FUR_B * (1.0 - s) + MUTE_B * s;
    /* hiss → darker burnt orange */
    if (g.pose_hiss > 0.2) {
        *r *= 1.0 - 0.08 * g.pose_hiss;
        *gg *= 1.0 - 0.22 * g.pose_hiss;
        *b *= 1.0 - 0.18 * g.pose_hiss;
    }
    /* pet glow — warmer */
    if (g.pose_pet > 0.2) {
        *r = fmin(1.0, *r + 0.06 * g.pose_pet);
        *gg = fmin(1.0, *gg + 0.03 * g.pose_pet);
    }
}

static void set_fur(cairo_t *cr, double a)
{
    double r, gg, b;
    fur_color(&r, &gg, &b);
    cairo_set_source_rgba(cr, r, gg, b, a);
}

static void set_ink(cairo_t *cr, double a)
{
    cairo_set_source_rgba(cr, INK_R, INK_G, INK_B, a);
}

static void draw_shadow_blob(cairo_t *cr, double ox, double oy, double sx, double sy)
{
    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, sx, sy);
    cairo_arc(cr, 0, 0, 1.0, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.05, 0.10, 0.08, 0.28);
    cairo_fill(cr);
    cairo_restore(cr);
}

static void draw_text_fade(cairo_t *cr, const char *s, double x, double y, double a)
{
    if (a <= 0.02)
        return;
    set_fur(cr, a);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 9);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
}

/* Waycat-style stub paw — small solid blob */
static void draw_paw(cairo_t *cr, double x, double y, double scale, int raised)
{
    cairo_save(cr);
    cairo_translate(cr, x, y);
    if (raised)
        cairo_rotate(cr, -0.35);
    cairo_scale(cr, scale * 1.05, scale * 0.95);
    cairo_arc(cr, 0, 0, 1.85, 0, 2 * G_PI);
    set_fur(cr, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);
}

/* Short limb ending in a paw */
static void draw_leg_paw(cairo_t *cr, double hx, double hy, double fx, double fy, double thick)
{
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, thick);
    set_fur(cr, 1.0);
    cairo_move_to(cr, hx, hy);
    cairo_line_to(cr, fx, fy);
    cairo_stroke(cr);
    draw_paw(cr, fx, fy + 0.7, 0.82 + thick * 0.02, 0);
}

static void draw_cat_body(cairo_t *cr)
{
    const double cx = SIZE * 0.5;
    const double cy = SIZE * 0.58;

    /* Squash only from real jump/hop activity — not idle wobble */
    double hop = 0;
    if (g.beh == B_JUMP && g.dur > 0.01) {
        double u = g.t / g.dur;
        if (u < 0.12)
            hop = -0.55; /* crouch */
        else if (u > 0.88)
            hop = -0.7;  /* land */
        else
            hop = 0.35 * fmin(1.0, g.air_z / 36.0);
    } else if (g.beh == B_FALL && g.air_z <= 2.0 && !g.land_ok) {
        hop = -0.55;
    } else if (g.beh == B_FALL && g.air_z <= 2.0 && g.land_ok) {
        hop = -0.4;
    } else if (g.motion == M_HOP && g.pose_walk > 0.35) {
        hop = sin(g.frame * 2.4) * g.pose_walk * 0.55;
    }
    double squash_x = 1.0 + hop * 0.22;
    double squash_y = 1.0 - hop * 0.26;
    if (g.pose_pet > 0.05) {
        double p = sin(g.t * 12.0);
        squash_x *= 1.0 + 0.12 * p * g.pose_pet;
        squash_y *= 1.0 - 0.12 * p * g.pose_pet;
    }

    double bob = sin(g.frame * 2.0) * g.pose_bob;
    if (g.pose_pet > 0.05)
        bob += sin(g.t * 12.0) * 2.0 * g.pose_pet;
    if (g.beh == B_TAILFLICK)
        bob += sin(g.t * 20.0) * 0.6 * smootherstep(fmin(1.0, g.blend_t / 0.25));
    if (hop > 0.05 && g.beh != B_JUMP)
        bob -= hop * 2.5;

    double fat = g.pose_fat;
    double lng = g.pose_long;
    double arch = g.pose_arch;
    double puff = g.pose_puff;
    double hs = g.pose_head;

    double sx = g.pose_sx * squash_x * (1.0 + puff * 0.18);
    double sy = g.pose_sy * squash_y * (1.0 + puff * 0.12);
    double sit = g.pose_sit;
    double roll = g.pose_roll;

    /* Travel heading still tracked; draw stays gravity-upright (feet down). */
    double face = (g.facing >= 0) ? 1.0 : -1.0;
    double body_rot;
    if (free_tumble_draw()) {
        /* allow full tumble for rolls / fail falls / twist jumps */
        double diff = angle_diff(g.angle, g.angle_vis);
        g.angle_vis += diff * 0.18;
        body_rot = g.twist + roll;
        if (g.beh == B_JUMP && g.anim_style == JUMP_TWIST)
            body_rot = g.twist;
        else if (g.beh == B_FLOP && g.anim_style == ROLL_SOMERSAULT)
            body_rot = g.twist;
        else if (g.beh == B_FLOP && g.anim_style == ROLL_BARREL)
            body_rot = g.twist;
    } else {
        /* mirror L/R + small pitch; never rotate onto back while walking */
        g.angle_vis += angle_diff(gravity_pitch(), g.angle_vis) * 0.28;
        body_rot = g.angle_vis + roll * 0.35 + g.twist * 0.15 + wall_cling_rot();
    }

    if (g.fade < 0.999)
        cairo_push_group(cr);
    cairo_save(cr);
    cairo_translate(cr, cx, cy + bob + sit - g.air_z);
    cairo_rotate(cr, body_rot);
    cairo_scale(cr, face * sx, sy);

    draw_shadow_blob(cr, 0, 17 + fat + g.air_z * 0.15,
                     12 + fat * 5 + lng * 3 - g.air_z * 0.08,
                     3.4 + fat * 1.5);

    /*
     * Side-view cat silhouette (local +X = nose):
     * round haunch → arched back → shoulder → deep chest → tucked belly.
     */
    double hx = -12.0 - fat * 1.8 + lng * 0.8;          /* haunch rear */
    double front = 13.5 + lng * 10.0;                    /* chest front */
    double chest_x = 7.0 + lng * 4.8;
    double back_y = -9.2 - arch * 9.5 - puff * 2.8;      /* spine peak */
    double shoulder_y = -7.4 - arch * 4.5;
    double rump_y = -6.6 - fat * 1.2 - arch * 2.0;
    double belly_y = 4.2 + fat * 3.2 - lng * 1.2;        /* higher = skinnier */
    double tuck = belly_y - (2.2 + lng * 1.4 + arch * 1.0); /* deeper tuck */
    double mid_h = (back_y + belly_y) * 0.5;
    double nose_x = front; /* legs / scratch still use nose_x */
    double butt_x = hx;

    /* Haunch mass — lean rear thigh */
    cairo_save(cr);
    cairo_translate(cr, hx + 4.0 + fat, mid_h * 0.12 + 0.2);
    cairo_scale(cr, 0.92 + fat * 0.2, 1.05 + fat * 0.15);
    cairo_arc(cr, 0, 0, 5.8 + fat * 1.2, 0, 2 * G_PI);
    set_fur(cr, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Chest mass — slimmer forechest */
    cairo_save(cr);
    cairo_translate(cr, chest_x + 1.2, 0.4);
    cairo_scale(cr, 1.05 + lng * 0.18, 0.92 + fat * 0.12);
    cairo_arc(cr, 0, 0, 5.2 + fat * 0.8, 0, 2 * G_PI);
    set_fur(cr, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Outer cat outline */
    cairo_new_path(cr);
    cairo_move_to(cr, hx, 1.0);
    /* rump curve up into back */
    cairo_curve_to(cr,
                   hx - 3.5 - fat, -1.0,
                   hx - 1.5, rump_y - 3.0,
                   hx + 4.0, rump_y);
    /* arched spine to mid-back */
    cairo_curve_to(cr,
                   hx + 9.0, back_y - 0.5,
                   -1.0 + lng, back_y - 1.8 - arch,
                   3.5 + lng * 3.0, back_y);
    /* withers → neck / shoulder */
    cairo_curve_to(cr,
                   7.5 + lng * 3.5, back_y + 0.8,
                   chest_x + 3.0, shoulder_y,
                   front - 1.5, -3.5);
    /* deep chest front — leaner profile */
    cairo_curve_to(cr,
                   front + 2.0, -0.8,
                   front + 1.4, belly_y - 1.6,
                   front - 3.0, belly_y - 0.4);
    /* belly with deeper mid tuck */
    cairo_curve_to(cr,
                   chest_x + 0.5, tuck + 0.2,
                   -2.0, belly_y + 1.0,
                   hx + 4.5, belly_y - 0.2);
    /* round under haunch */
    cairo_curve_to(cr,
                   hx + 1.2, belly_y - 1.8,
                   hx - 2.2 - fat, 3.5,
                   hx, 1.0);
    cairo_close_path(cr);

    set_fur(cr, 1.0);
    cairo_fill_preserve(cr);
    /* Soft Waycat edge — no dark outline */
    {
        double r, gg, b;
        fur_color(&r, &gg, &b);
        cairo_set_source_rgba(cr, r * 0.75, gg * 0.75, b * 0.75, 0.35);
    }
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    /* Soft shoulder / hip landmarks */
    {
        double r, gg, b;
        fur_color(&r, &gg, &b);
        cairo_set_source_rgba(cr, r * 0.85, gg * 0.88, b * 0.85, 0.22);
    }
    cairo_arc(cr, chest_x - 0.5, -2.5, 2.4, 0, 2 * G_PI);
    cairo_fill(cr);
    cairo_arc(cr, hx + 5.5, -1.0, 2.8 + fat, 0, 2 * G_PI);
    cairo_fill(cr);

    /* Fluff spikes when puffed */
    if (puff > 0.15) {
        set_fur(cr, 0.7 * puff);
        for (int i = 0; i < 5; i++) {
            double fx = -6 + i * 5.0;
            double fy = back_y - 1;
            cairo_move_to(cr, fx, fy);
            cairo_line_to(cr, fx + 1.5, fy - 5 - puff * 4);
            cairo_line_to(cr, fx + 3.0, fy);
            cairo_fill(cr);
        }
    }

    /* Head — Waycat proportions */
    double head_y = -11.5 + 5.0 * g.head_drop - arch * 2.5;
    double head_x = 8.0 + lng * 5.5;
    double hr = (10.8 + fat * 0.4 + puff * 1.0) * hs;
    double sleep_a = g.pose_sleep;
    double narrow = g.pose_narrow;
    double cute = 1.0 - 0.55 * g.pose_hiss;

    /* Neck bridge so head isn't floating */
    {
        cairo_new_path(cr);
        cairo_move_to(cr, head_x - 5.0, head_y + 4.0);
        cairo_curve_to(cr, head_x - 2.0, head_y + 8.0,
                       chest_x + 2.0, shoulder_y + 2.0,
                       chest_x + 4.0, -1.0);
        cairo_curve_to(cr, chest_x + 6.0, 2.0,
                       head_x + 4.0, head_y + 7.0,
                       head_x + 3.5, head_y + 3.0);
        cairo_close_path(cr);
        set_fur(cr, 1.0);
        cairo_fill(cr);
    }

    cairo_save(cr);
    cairo_translate(cr, head_x, head_y);
    cairo_scale(cr, hs * (1.0 + puff * 0.1), hs * (1.0 - g.head_drop * 0.08));

    /* Round soft skull — Waycat head blob */
    cairo_save(cr);
    cairo_scale(cr, 1.12, 1.0);
    cairo_arc(cr, 0, 1.0, hr * 0.88, 0, 2 * G_PI);
    set_fur(cr, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Soft cheeks — same fur */
    {
        double cheek = 0.35 + 0.35 * g.pose_pet + 0.12 * puff;
        set_fur(cr, 1.0);
        cairo_arc(cr, -5.8, 3.6, 4.2 + cheek, 0, 2 * G_PI);
        cairo_fill(cr);
        cairo_arc(cr, 6.0, 3.4, 4.0 + cheek * 0.9, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    /* Tiny muzzle bump */
    cairo_save(cr);
    cairo_scale(cr, 1.15, 0.75);
    cairo_arc(cr, 0.4, 5.8, 3.2, 0, 2 * G_PI);
    set_fur(cr, 1.0);
    cairo_fill(cr);
    cairo_restore(cr);

    /* Pointed Waycat ears + dark inner punch */
    {
        double alert = 1.0 - sleep_a * 0.45;
        double ear_h = -16.0 * alert - 0.5;
        double ear_spread = 1.02 + puff * 0.22;
        double pin = g.pose_hiss;
        if (pin > 0.25) {
            ear_h = -9.0;
            ear_spread *= 1.0 + pin * 0.18;
        }

        double tw_l = g.ear_l;
        double tw_r = g.ear_r;
        double ear_h_l = ear_h - tw_l * 3.2;
        double ear_h_r = ear_h - tw_r * 3.2;
        double fold_l = pin * 2.5 + tw_l * 2.0;
        double fold_r = pin * 2.0 + tw_r * 2.0;
        double spr_l = ear_spread + tw_l * 0.06;
        double spr_r = ear_spread + tw_r * 0.06;

        cairo_new_path(cr);
        cairo_move_to(cr, -8.2 * spr_l, -3.2);
        cairo_line_to(cr, -4.8 * spr_l - fold_l, ear_h_l);
        cairo_line_to(cr, -1.6, -5.2);
        cairo_close_path(cr);
        cairo_move_to(cr, 1.8, -5.2);
        cairo_line_to(cr, 5.0 * spr_r + fold_r, ear_h_r);
        cairo_line_to(cr, 8.4 * spr_r, -3.0);
        cairo_close_path(cr);
        set_fur(cr, 1.0);
        cairo_fill(cr);

        /* dark inner-ear cutouts (Waycat negative space) */
        set_ink(cr, 0.55 + 0.2 * sleep_a);
        cairo_new_path(cr);
        cairo_move_to(cr, -6.0 * spr_l, -4.2);
        cairo_line_to(cr, -4.6 * spr_l - fold_l * 0.5, ear_h_l + 3.5);
        cairo_line_to(cr, -2.8, -5.0);
        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_new_path(cr);
        cairo_move_to(cr, 3.0, -5.0);
        cairo_line_to(cr, 4.8 * spr_r + fold_r * 0.4, ear_h_r + 3.4);
        cairo_line_to(cr, 6.2 * spr_r, -4.0);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    /* Waycat eyes — dark punched dots that track activity */
    if (sleep_a > 0.55 || (g.pose_roll > 0.3 && sleep_a > 0.2)) {
        set_ink(cr, 0.55);
        cairo_set_line_width(cr, 1.6);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, -6.2, 1.4);
        cairo_curve_to(cr, -3.6, 3.2, -1.4, 3.2, 0.6, 1.4);
        cairo_stroke(cr);
        cairo_move_to(cr, 2.0, 1.4);
        cairo_curve_to(cr, 4.4, 3.2, 6.4, 3.2, 8.2, 1.4);
        cairo_stroke(cr);
    } else if (g.eyes_closed) {
        set_ink(cr, 0.5);
        cairo_set_line_width(cr, 1.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, -6.0, 1.5); cairo_curve_to(cr, -3.4, 2.8, -1.2, 2.8, 0.6, 1.5);
        cairo_move_to(cr, 2.2, 1.5); cairo_curve_to(cr, 4.4, 2.8, 6.4, 2.8, 8.0, 1.5);
        cairo_stroke(cr);
    } else {
        double er = (2.15 - 0.55 * narrow) * (1.0 + 0.12 * g.pose_pet);
        double dil = fmax(0.35, fmin(1.15, 0.55 + 0.55 * g.pupil));
        er *= dil;
        double ex0 = -3.8 - narrow * 0.2;
        double ex1 = 4.6 + narrow * 0.2;
        double ey = 1.2;
        double gx = g.gaze_x * 1.6;
        double gy = g.gaze_y * 1.2;
        if (g.pose_hiss > 0.35) {
            set_ink(cr, 0.95);
            cairo_save(cr);
            cairo_translate(cr, ex0 + gx, ey + gy);
            cairo_scale(cr, 0.35, 1.15);
            cairo_arc(cr, 0, 0, er, 0, 2 * G_PI);
            cairo_fill(cr);
            cairo_restore(cr);
            cairo_save(cr);
            cairo_translate(cr, ex1 + gx, ey + gy);
            cairo_scale(cr, 0.35, 1.15);
            cairo_arc(cr, 0, 0, er, 0, 2 * G_PI);
            cairo_fill(cr);
            cairo_restore(cr);
        } else {
            set_ink(cr, 0.92);
            cairo_arc(cr, ex0 + gx, ey + gy, er, 0, 2 * G_PI);
            cairo_fill(cr);
            cairo_arc(cr, ex1 + gx, ey + gy, er, 0, 2 * G_PI);
            cairo_fill(cr);
        }
    }

    /* Tiny dark nose */
    {
        double nx = 0.5, ny = 5.6;
        set_ink(cr, 0.55 + 0.25 * g.pose_hiss);
        cairo_arc(cr, nx, ny, 1.15, 0, 2 * G_PI);
        cairo_fill(cr);
        if (g.pose_hiss > 0.25) {
            cairo_set_line_width(cr, 1.2);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, nx - 2.0, ny + 1.8);
            cairo_curve_to(cr, nx - 0.6, ny + 4.0, nx + 0.6, ny + 4.0, nx + 2.0, ny + 1.8);
            cairo_stroke(cr);
        } else if (g.pose_pet > 0.2 || cute > 0.7) {
            cairo_set_line_width(cr, 1.1);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            set_ink(cr, 0.35 + 0.25 * g.pose_pet);
            cairo_move_to(cr, nx - 2.2, ny + 1.6);
            cairo_curve_to(cr, nx - 0.8, ny + 2.8, nx + 0.8, ny + 2.8, nx + 2.2, ny + 1.6);
            cairo_stroke(cr);
        }
    }

    /* Sparse whiskers */
    if (sleep_a < 0.75) {
        set_ink(cr, 0.22 * cute + 0.08);
        cairo_set_line_width(cr, 1.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        double wy = 4.8;
        cairo_move_to(cr, -4.5, wy - 0.6); cairo_line_to(cr, -11.5, wy - 1.8);
        cairo_move_to(cr, -4.6, wy + 0.5); cairo_line_to(cr, -12.0, wy + 0.4);
        cairo_move_to(cr, 5.0, wy - 0.6); cairo_line_to(cr, 12.0, wy - 1.6);
        cairo_move_to(cr, 5.1, wy + 0.5); cairo_line_to(cr, 12.5, wy + 0.5);
        cairo_stroke(cr);
    }

    cairo_restore(cr); /* head */

    /* Tail — multi-segment fluid ribbon (mesmerizing wave toward tip) */
    {
        double energy = fmax(0.18, g.pose_wag);
        if (g.pose_walk > 0.05)
            energy = fmax(energy, 0.45 + 0.4 * g.pose_walk);
        if (g.beh == B_TAILFLICK)
            energy = fmax(energy, 1.15);
        if (g.pose_pet > 0.05)
            energy = fmax(energy, 0.55 + 0.35 * g.pose_pet);
        if (g.pose_hiss > 0.05)
            energy = fmax(energy, 0.85 + 0.3 * g.pose_hiss);
        if (sleep_a > 0.5)
            energy *= 0.35 + 0.2 * (1.0 - sleep_a);

        double t = g.tail_t;
        /* layered waves: slow base sway + mid ripple + tip flick */
        double base = sin(t * 1.7) * 0.55 + sin(t * 0.6 + 1.1) * 0.25;
        double mid = sin(t * 3.4 + 0.7) * 0.4 + sin(t * 5.1) * 0.15;
        double tip = sin(t * 7.2 + 0.3) * 0.55 + sin(t * 11.0) * 0.18;
        if (g.beh == B_TAILFLICK) {
            tip += sin(t * 18.0) * 0.7;
            mid += sin(t * 14.0) * 0.35;
        }
        if (g.pose_hiss > 0.2) {
            tip += sin(t * 22.0) * 0.45 * g.pose_hiss;
            base *= 0.6;
        }

        double amp = (10.0 + fat * 2.0) * energy;
        double len = 22.0 + fat * 3.0 + (g.beh == B_TAILFLICK ? 6.0 : 0);
        double root_x = butt_x + 1.5;
        double root_y = mid_h - 1.0;

        /* spine samples */
        enum { TAIL_N = 10 };
        double px[TAIL_N], py[TAIL_N];
        double ang = -0.55 + base * 0.55 * energy; /* mostly up-back */
        px[0] = root_x;
        py[0] = root_y;
        for (int i = 1; i < TAIL_N; i++) {
            double u = (double)i / (TAIL_N - 1);
            double u2 = u * u;
            /* amplitude grows toward tip — classic fluid whip */
            double wave = base * (0.35 + 0.3 * u)
                        + mid * u
                        + tip * u2;
            double curl = sin(t * 2.2 + u * 3.5) * 0.2 * u * energy;
            ang += (-0.08 + wave * 0.55 * energy + curl) * (0.55 + 0.45 * u);
            double seg = (len / (TAIL_N - 1)) * (1.0 - 0.12 * u);
            px[i] = px[i - 1] + cos(ang) * -seg; /* -X is behind */
            py[i] = py[i - 1] + sin(ang) * -seg;
            /* lift bias */
            py[i] -= (0.6 + amp * 0.04) * u;
            px[i] += sin(t * 2.8 + u * 4.0) * amp * 0.08 * u2;
            py[i] += cos(t * 3.1 + u * 3.2) * amp * 0.12 * u;
        }

        /* tapered ribbon outline */
        cairo_new_path(cr);
        cairo_move_to(cr, px[0], py[0] - 2.2);
        for (int i = 1; i < TAIL_N; i++) {
            double u = (double)i / (TAIL_N - 1);
            double half = 2.4 * (1.0 - u * 0.82);
            double dx = px[i] - px[i - 1];
            double dy = py[i] - py[i - 1];
            double inv = hypot(dx, dy);
            if (inv < 1e-3) inv = 1;
            double nx = -dy / inv, ny = dx / inv;
            cairo_line_to(cr, px[i] + nx * half, py[i] + ny * half);
        }
        /* tip bulb */
        cairo_curve_to(cr,
                       px[TAIL_N - 1] - 1.5, py[TAIL_N - 1] - 2.5,
                       px[TAIL_N - 1] - 3.0, py[TAIL_N - 1] + 0.5,
                       px[TAIL_N - 1] - 0.5, py[TAIL_N - 1] + 2.0);
        for (int i = TAIL_N - 1; i >= 1; i--) {
            double u = (double)i / (TAIL_N - 1);
            double half = 2.4 * (1.0 - u * 0.82);
            double dx = px[i] - px[i - 1];
            double dy = py[i] - py[i - 1];
            double inv = hypot(dx, dy);
            if (inv < 1e-3) inv = 1;
            double nx = -dy / inv, ny = dx / inv;
            cairo_line_to(cr, px[i] - nx * half, py[i] - ny * half);
        }
        cairo_line_to(cr, px[0], py[0] + 2.2);
        cairo_close_path(cr);
        set_fur(cr, 1.0);
        cairo_fill(cr);

        /* soft tip bulb */
        {
            double r, gg, b;
            fur_color(&r, &gg, &b);
            cairo_set_source_rgba(cr, r * 1.05, gg * 1.05, b * 1.05, 0.35 * energy);
        }
        cairo_arc(cr, px[TAIL_N - 1], py[TAIL_N - 1], 2.2, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    if (sleep_a > 0.7) {
        draw_text_fade(cr, "z", head_x + 14, head_y - 16, 0.45 * sleep_a);
        draw_text_fade(cr, "z", head_x + 20, head_y - 24, 0.35 * sleep_a);
    }

    if (g.pose_groom > 0.05) {
        set_fur(cr, 0.95 * g.pose_groom);
        cairo_arc(cr, 2, 4 + sin(g.t * 10) * 3, 5.5, 0, 2 * G_PI);
        cairo_fill(cr);
    }

    /* Proper paws — walk / wall-climb / stairs / sit / scratch */
    {
        double walk = g.pose_walk;
        double sit_amt = fmax(0.0, g.pose_sit) / 4.5;
        double vclimb = vertical_travel() * walk;
        int on_wall = near_side_wall() && vclimb > 0.4;
        int on_stairs = !near_side_wall() && vclimb > 0.4;

        if (walk > 0.08 && on_wall) {
            /* Wall climb — paws reach along the wall (body local “up”) */
            double phase = g.frame * 3.6;
            double a = sin(phase);
            double b = sin(phase + G_PI);
            double reach = 8.0 + vclimb * 6.0;
            draw_paw(cr, 4.0 + a * 2.0, belly_y - reach * (0.55 + 0.35 * a), 0.72, 1);
            draw_paw(cr, 9.0 - a * 1.5, belly_y - reach * (0.4 + 0.35 * -a), 0.68, 1);
            draw_paw(cr, -5.0 + b * 2.0, belly_y - reach * (0.25 + 0.3 * b), 0.66, 0);
            draw_paw(cr, -10.0 - b * 1.5, belly_y - reach * (0.15 + 0.25 * -b), 0.62, 0);
        } else if (walk > 0.08 && on_stairs) {
            /* Stair steps — high vertical foot lift */
            double phase = g.frame * 5.5;
            double a = sin(phase);
            double b = sin(phase + G_PI);
            double lift = 4.0 + vclimb * 7.0;
            double leg = 5.0 + walk * 4.0;
            double thick = 2.15;
            draw_leg_paw(cr, 5.0, belly_y - 1.0,
                         6.0 + a * 3.0, belly_y + leg - fmax(0.0, a) * lift, thick);
            draw_leg_paw(cr, 9.0, belly_y - 0.5,
                         10.0 - a * 2.5, belly_y + leg - fmax(0.0, -a) * lift, thick * 0.92);
            draw_leg_paw(cr, -6.0, belly_y - 1.0,
                         -7.0 + b * 3.0, belly_y + leg - fmax(0.0, b) * lift * 0.85, thick);
            draw_leg_paw(cr, -10.0, belly_y - 0.5,
                         -11.0 - b * 2.5, belly_y + leg - fmax(0.0, -b) * lift * 0.85, thick * 0.92);
        } else if (walk > 0.08) {
            /* Diagonal-aware stride — longer reach + lift on 45° runs */
            double cadence = 2.4;
            if (g.motion == M_SPRINT || g.beh == B_DASH)
                cadence = 3.3;
            else if (g.motion == M_TROT || g.motion == M_DIAGONAL)
                cadence = 2.85;
            double phase = g.frame * cadence;
            double a = sin(phase);
            double b = sin(phase + G_PI);
            double leg_len = 5.5 + walk * 5.5;
            double thick = 2.1 + (1.0 - lng) * 0.35;
            double diag = 0.0;
            {
                double spd = hypot(g.vx, g.vy);
                if (spd > 12.0)
                    diag = fabs(sin(2.0 * atan2(g.vy, g.vx)));
            }
            double stride = 5.2 + diag * 2.4 + walk * 0.8;
            double lift = 1.4 + diag * 1.6;

            draw_leg_paw(cr, 5.0, belly_y - 1.0,
                         6.0 + a * stride, belly_y - 1.0 + leg_len + a * lift, thick);
            draw_leg_paw(cr, 9.0, belly_y - 0.5,
                         10.0 - a * (stride * 0.85), belly_y - 0.5 + leg_len - a * lift * 0.85, thick * 0.92);
            draw_leg_paw(cr, -6.0, belly_y - 1.0,
                         -7.0 + b * stride, belly_y - 1.0 + leg_len + b * lift, thick);
            draw_leg_paw(cr, -10.0, belly_y - 0.5,
                         -11.0 - b * (stride * 0.8), belly_y - 0.5 + leg_len - b * lift * 0.8, thick * 0.92);

            if (g.motion == M_HOP && a > 0.55)
                draw_paw(cr, 7.0 + a * 2.0, belly_y + 2.0, 0.85, 1);
        } else if (g.beh == B_SCRATCH && g.anim_style == SCR_POST) {
            /* reared — front paws rake up */
            draw_paw(cr, nose_x - 2, -8 + sin(g.t * 16) * 3, 0.78, 1);
            draw_paw(cr, nose_x + 4, -6 + cos(g.t * 16) * 3, 0.74, 1);
            draw_leg_paw(cr, -5, belly_y, -6, belly_y + 7, 2.2);
            draw_leg_paw(cr, -9, belly_y, -10, belly_y + 6.5, 2.05);
        } else if (g.beh == B_SCRATCH && g.anim_style == SCR_DIG) {
            draw_leg_paw(cr, 4, belly_y, 2 + sin(g.t * 18) * 4, belly_y + 8, 2.15);
            draw_leg_paw(cr, 8, belly_y, 9 - sin(g.t * 18) * 4, belly_y + 7.5, 2.0);
            draw_paw(cr, -7, belly_y + 5, 0.68, 0);
            draw_paw(cr, -11, belly_y + 4.5, 0.64, 0);
        } else if (sleep_a < 0.65) {
            double tuck = 1.0 - sit_amt * 0.35;
            double py = belly_y + 1.5 + sit_amt * 1.5;
            draw_paw(cr, -3.5 * tuck, py, 0.68 + fat * 0.1, 0);
            draw_paw(cr, 2.5 * tuck, py + 0.3, 0.7 + fat * 0.08, 0);
            if (sit_amt < 0.55) {
                draw_paw(cr, -8.5, py - 0.5, 0.62, 0);
                draw_paw(cr, 6.5, py - 0.2, 0.6, 0);
            }
        } else if (sleep_a >= 0.65) {
            draw_paw(cr, 1.0, belly_y + 2.0, 0.64, 0);
            draw_paw(cr, -4.0, belly_y + 1.5, 0.58, 0);
        }
    }

    if (g.beh == B_SCRATCH && g.t > 0.2) {
        double a = fmin(1.0, g.blend_t / 0.15);
        set_fur(cr, 0.65 * a);
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

    if (g.pose_pet > 0.05) {
        double a = 0.4 * g.pose_pet;
        if (g.anim_style == PET_BOUNCE) {
            draw_text_fade(cr, "<3", -10, -36 - g.t * 10, a);
            draw_text_fade(cr, "<3", 6, -42 - g.t * 7, a * 0.7);
            draw_text_fade(cr, "<3", -2, -50 - sin(g.t * 8) * 4, a * 0.55);
        } else if (g.anim_style == PET_RUB) {
            draw_text_fade(cr, "~", head_x + 10, head_y - 14, a * 0.8);
            draw_text_fade(cr, "<3", -6, -32 - fabs(sin(g.t * 5)) * 6, a * 0.65);
        } else if (g.anim_style == PET_BELLY) {
            draw_text_fade(cr, "<3", -4, 8, a * 0.7);
            draw_text_fade(cr, "uwu", -10, -28, a * 0.5);
        } else if (g.anim_style == PET_NUZZLE) {
            draw_text_fade(cr, "<3", head_x + 8, head_y - 18 - g.t * 5, a);
        } else { /* LOAF */
            draw_text_fade(cr, "z", head_x + 12, head_y - 14, a * 0.35);
            draw_text_fade(cr, "<3", -8, -30, a * 0.45);
        }
    }
    if (g.pose_hiss > 0.1) {
        draw_text_fade(cr, "!", head_x + 12, head_y - 18, 0.75 * g.pose_hiss);
        draw_text_fade(cr, "HSS", head_x + 8, head_y - 6, 0.5 * g.pose_hiss);
    }
    if (g.beh == B_LOOK)
        draw_text_fade(cr, "...", head_x + 12, head_y - 20, 0.4 * smootherstep(g.blend_t / 0.35));

    cairo_restore(cr); /* body transform */
    if (g.fade < 0.999) {
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, g.fade <= 0 ? 0.0 : g.fade);
    }
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
        read_cursor(&g.cx, &g.cy);

        /* Cooling off — usually confused, rarely actually mad */
        if (g.pet_sulk > 0.0 || g.overstim_left > 0) {
            g.mood = fmax(-1.0, g.mood - 0.05);
            if (g.beh != B_DASH && g.beh != B_SCRATCH)
                set_beh((frand() < 0.65) ? B_LOOK : B_HISS, 0.5 + frand() * 0.4);
            g.nap_after = 3.0 + frand() * 7.0;
            queue_draw_cat();
            return TRUE;
        }

        /* Pet streak: loves attention… until the brain cell flips */
        if (g.pet_age < 7.0)
            g.pet_streak++;
        else
            g.pet_streak = 1;
        g.pet_age = 0;

        int limit = 5 + (g.mood > 0.2 ? 2 : 0); /* clingy */
        if (g.pet_streak >= limit) {
            begin_overstim_flee();
            queue_draw_cat();
            return TRUE;
        }

        /* Instant softie mode */
        g.mood = fmin(1.0, g.mood + 0.28);
        wake_then_nap(B_ACCEPT, 1.6 + frand() * 1.0);
        queue_draw_cat();
        return TRUE;
    }
    if (e->button == 2 || e->button == 3) {
        /* Poke → derpy chaos */
        wake_random_activity();
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
            g.vz = cos(u * G_PI) * 54.0 * G_PI / fmax(g.dur, 0.2);
        } else if (g.anim_style == JUMP_LEAP) {
            h = sin(u * G_PI) * 42.0;
            g.vz = cos(u * G_PI) * 42.0 * G_PI / fmax(g.dur, 0.2);
            move_styled(g.tx + SIZE * 0.5, g.ty + SIZE * 0.55, dt);
        } else if (g.anim_style == JUMP_TWIST) {
            h = sin(u * G_PI) * 48.0;
            g.vz = cos(u * G_PI) * 48.0 * G_PI / fmax(g.dur, 0.2);
            g.twist += dt * 14.0;
            /* keep travel facing; twist is visual-only under gravity draw */
        } else { /* DOUBLE */
            if (u < 0.5)
                h = sin(u * 2.0 * G_PI) * 40.0;
            else
                h = sin((u - 0.5) * 2.0 * G_PI) * 28.0;
            g.vz = (u < 0.5)
                ? cos(u * 2.0 * G_PI) * 80.0 * G_PI / fmax(g.dur, 0.2)
                : cos((u - 0.5) * 2.0 * G_PI) * 56.0 * G_PI / fmax(g.dur, 0.2);
            g.x += cos(g.angle) * 40.0 * dt;
            g.y += sin(g.angle) * 40.0 * dt;
            apply_margins();
        }
        g.air_z = h;
        g.frame += dt * 10.0;
        return;
    }

    if (g.beh == B_FALL) {
        /* Real gravity on air_z; horizontal drift from last velocity / facing */
        g.vz -= GRAVITY_PX * dt;
        g.air_z += g.vz * dt;
        double drift = (g.facing >= 0) ? 1.0 : -1.0;
        if (g.anim_style == FALL_TUMBLE) {
            g.twist += dt * 12.0;
            g.x += drift * 55.0 * dt + g.vx * 0.2 * dt;
        } else if (g.anim_style == FALL_BELLY) {
            g.twist = sin(g.t * 3.0) * 0.4;
            g.x += drift * 25.0 * dt;
        } else {
            /* grace: slight righting */
            g.twist *= exp(-4.0 * dt);
            g.x += drift * 15.0 * dt;
        }
        g.y += g.vy * 0.15 * dt;
        apply_margins();

        if (g.air_z <= 0.0) {
            g.air_z = 0;
            /* impact bounce once for fails */
            if (g.vz < -120.0 && g.path_u < 0.5) {
                g.vz = -g.vz * (g.land_ok ? 0.18 : 0.35);
                g.air_z = 2.0;
                g.path_u = 1.0; /* mark bounced */
            } else {
                g.vz = 0;
                /* finish after a short recover window */
                if (g.t < g.dur - 0.55)
                    g.t = g.dur - 0.55;
            }
        }
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
            /* tiny jitter — do not spin travel angle (keeps feet-down facing) */
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

    /* Hover wake while sleeping → brief stir, then nap timer */
    if (g.beh == B_SLEEP) {
        double dx = g.cx - cat_cx();
        double dy = g.cy - cat_cy();
        if (hypot(dx, dy) < SIZE * 0.55) {
            face_toward(g.cx, g.cy);
            wake_then_nap((frand() < 0.5) ? B_LOOK : B_SIDE_EYE, 1.2 + frand() * 1.5);
            queue_draw_cat();
            return G_SOURCE_CONTINUE;
        }
    }

    g.t += dt;
    g.bob += dt;
    g.blink_t += dt;
    g.tail_t += dt * (1.0 + 0.35 * g.pose_wag + 0.5 * g.pose_walk
                      + (g.beh == B_TAILFLICK ? 1.8 : 0)
                      + 0.4 * g.pose_hiss);
    g.pet_age += dt;
    if (g.pet_sulk > 0.0) {
        g.pet_sulk -= dt;
        if (g.pet_sulk < 0.0)
            g.pet_sulk = 0.0;
    }
    if (g.pet_age > 7.0 && g.pet_streak > 0)
        g.pet_streak = 0;

    /* Interaction / stir budget expired → back to sleep */
    if (g.nap_after > 0.0 && g.beh != B_SLEEP && g.overstim_left <= 0
        && g.beh != B_DASH && g.pet_sulk <= 0.0) {
        g.nap_after -= dt;
        if (g.nap_after <= 0.0) {
            go_to_sleep(25.0 + frand() * 50.0);
            queue_draw_cat();
            return G_SOURCE_CONTINUE;
        }
    }

    if (is_stunt(g.beh)) {
        tick_stunt(dt);
        update_pose_targets(dt);
        queue_draw_cat();
        if (g.t >= g.dur) {
            double end_h = g.air_z;
            double end_vz = g.vz;
            g.air_z = 0;
            g.vz = 0;
            g.twist = 0;
            if (g.after_beh >= 0) {
                Behavior next = (Behavior)g.after_beh;
                double d = g.after_dur;
                g.after_beh = -1;
                set_beh(next, d);
            } else if (g.beh == B_SCRATCH && g.overstim_left > 0) {
                continue_overstim_or_flee();
            } else if (g.beh == B_JUMP) {
                /* Random miss-land → gravity fall; else stuck stick */
                if (frand() < 0.32)
                    begin_fall(fmax(22.0, end_h + 8.0), fmax(30.0, end_vz * 0.35), -1);
                else
                    set_beh((frand() < 0.5) ? B_LOAF : B_STRETCH, 1.0 + frand());
            } else if (g.beh == B_FALL) {
                if (g.land_ok)
                    set_beh((frand() < 0.6) ? B_LOAF : B_STRETCH, 1.0 + frand());
                else
                    set_beh((frand() < 0.5) ? B_LOOK : B_HISS, 0.9 + frand());
            } else if (g.beh == B_FLOP) {
                if (frand() < 0.22)
                    begin_fall(20.0 + frand() * 30.0, 60.0 + frand() * 40.0, FALL_TUMBLE);
                else
                    set_beh((frand() < 0.5) ? B_LOAF : B_LOOK, 1.2 + frand());
            } else
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

    /* Draw when fading, blending, moving, or tail flowing */
    int need_draw = (g.fade < 0.999) || (g.blend_t < g.blend_dur + 0.08)
                    || (hypot(g.vx, g.vy) > 4.0)
                    || ((int)(g.tail_t * 16.0) != (int)((g.tail_t - dt) * 16.0));

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
    case B_FLOP:
        if (((int)(g.bob * 3)) != ((int)((g.bob - dt) * 3)))
            need_draw = 1;
        break;

    case B_SLEEP:
        if (g.ear_phase > 0.0 || g.ear_l > 0.02 || g.ear_r > 0.02)
            need_draw = 1;
        else if (((int)(g.bob * 3)) != ((int)((g.bob - dt) * 3)))
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

    case B_ACCEPT: {
        /* Animate the chosen affection style */
        if (g.anim_style == PET_BOUNCE) {
            g.air_z = fabs(sin(g.t * 11.0)) * 10.0;
            g.frame += dt * 12.0;
        } else if (g.anim_style == PET_RUB) {
            /* figure-8 cheek rub in place toward cursor */
            double u = g.t * 2.2;
            double ox = sin(u) * 16.0;
            double oy = sin(u * 2.0) * 6.0;
            g.x = g.ox - SIZE * 0.5 + ox;
            g.y = g.oy - SIZE * 0.55 + oy;
            smooth_turn_to(atan2(g.cy - cat_cy(), g.cx - cat_cx()), dt, 6.0);
            apply_margins();
            g.frame += dt * 10.0;
        } else if (g.anim_style == PET_BELLY) {
            g.air_z *= exp(-5.0 * dt);
            g.twist = sin(g.t * 2.5) * 0.12;
            g.frame += dt * 7.0;
        } else if (g.anim_style == PET_NUZZLE) {
            smooth_turn_to(atan2(g.cy - cat_cy(), g.cx - cat_cx()), dt, 5.0);
            /* lean a little toward cursor */
            double dx = g.cx - cat_cx();
            double dy = g.cy - cat_cy();
            double len = hypot(dx, dy);
            if (len > 1.0) {
                g.x += (dx / len) * 18.0 * dt;
                g.y += (dy / len) * 10.0 * dt;
                apply_margins();
            }
            g.air_z = 2.0 + sin(g.t * 6.0) * 1.5;
            g.frame += dt * 8.0;
        } else { /* PET_LOAF */
            g.air_z *= exp(-6.0 * dt);
            g.eyes_closed = (fmod(g.t, 1.4) > 1.05) ? 1 : 0;
            g.frame += dt * 4.0;
        }
        need_draw = 1;
        break;
    }

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
            /* Still within nap window — tiny follow-up, else sleep */
            if (g.nap_after > 0.8)
                set_beh(B_GROOM, fmin(1.2, g.nap_after * 0.5));
            else
                go_to_sleep(25.0 + frand() * 40.0);
            need_draw = 1;
        } else if (g.beh == B_DASH && g.pet_sulk > 0.5) {
            go_to_sleep(30.0 + frand() * 40.0);
            need_draw = 1;
        } else if (g.beh == B_SLEEP) {
            /* Sleep ended on its own — almost always nap again */
            if (frand() < 0.85) {
                go_to_sleep(20.0 + frand() * 50.0);
            } else {
                wake_random_activity();
            }
            need_draw = 1;
        } else if (is_locomotion(g.beh)) {
            if (g.after_beh == B_SLEEP) {
                Behavior next = (Behavior)g.after_beh;
                double d = g.after_dur;
                g.after_beh = -1;
                set_beh(next, d);
            } else if (g.nap_after > 0.5) {
                set_beh(B_LOAF, fmin(1.5, g.nap_after * 0.4));
            } else {
                go_to_sleep(25.0 + frand() * 45.0);
            }
            need_draw = 1;
        } else {
            if (g.nap_after > 0.6)
                wake_random_activity(); /* refresh within remaining window */
            else
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
    g.vz = 0;
    g.air_z = 0;
    g.land_ok = 1;
    g.motion = M_WALK;
    g.mood = 0.35; /* sunny little menace */
    g.after_beh = -1;
    g.pet_streak = 0;
    g.pet_age = 99.0;
    g.overstim_left = 0;
    g.pet_sulk = 0;
    g.nap_after = 0;
    g.nrecent = 0;
    g.last_motion = M_COUNT;
    g.obscured = 0;
    g.fade = 1.0;
    g.prev_beh = B_SLEEP;
    g.blend_t = 1;
    g.blend_dur = 0.01;
    g.pose_sx = 1;
    g.pose_sy = 1;
    g.pose_wag = 0.35;
    g.tail_t = 0;
    g.gaze_x = 0;
    g.gaze_y = 0;
    g.pupil = 0.55;
    g.ear_l = 0;
    g.ear_r = 0;
    g.ear_phase = 0;
    g.ear_which = 0;
    g.ear_wait = 8.0 + frand() * 12.0; /* first sleep twitch after a while */
    g.pose_fat = 0.10;
    g.pose_long = 0.58;
    g.pose_arch = 0.08;
    g.pose_head = 1.0;
    g.mon_w = 1920;
    g.mon_h = 1080;

    refresh_monitor_size();
    read_cursor(&g.cx, &g.cy);
    /* Start already napping on a side */
    pick_sleep_side();
    g.x = g.tx;
    g.y = g.ty;
    clamp_pos();
    set_beh(B_SLEEP, 35.0 + frand() * 40.0);

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
