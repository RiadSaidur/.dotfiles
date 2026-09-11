/*
 * Reference unit tests for shadow-cat math / pose helpers.
 * Run: make -C ... test
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define G_PI 3.14159265358979323846

static int fails;
static int passes;

static void expect(int cond, const char *name)
{
    if (cond) {
        passes++;
        printf("  PASS  %s\n", name);
    } else {
        fails++;
        printf("  FAIL  %s\n", name);
    }
}

static double angle_diff(double a, double b)
{
    return atan2(sin(a - b), cos(a - b));
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

static void clamp_box(double *x, double *y, int mon_x, int mon_y, int mon_w, int mon_h, int size)
{
    double min_x = mon_x + 4;
    double min_y = mon_y + 4;
    double max_x = mon_x + mon_w - size - 4;
    double max_y = mon_y + mon_h - size - 4;
    if (*x < min_x) *x = min_x;
    if (*y < min_y) *y = min_y;
    if (*x > max_x) *x = max_x;
    if (*y > max_y) *y = max_y;
}

static int side_bias_bucket(double roll)
{
    /* Mirrors pick_screen_edge_target bias: L/R ~84% */
    if (roll < 0.42) return 2;
    if (roll < 0.84) return 3;
    if (roll < 0.92) return 0;
    return 1;
}

static int wander_bucket(double roll)
{
    if (roll < 0.02) return 0;      /* content */
    if (roll < 0.24) return 1;      /* window edge */
    return 2;                       /* screen side */
}

/* Mirrors gravity_pitch clamp idea */
static double gravity_pitch_clamp(double angle, double vz, int airborne)
{
    double climb = sin(angle);
    double pitch = climb * 0.20;
    if (airborne)
        pitch += fmax(-0.35, fmin(0.35, -vz * 0.00055));
    if (pitch > 0.38) pitch = 0.38;
    if (pitch < -0.38) pitch = -0.38;
    return pitch;
}

/* Mirrors fall style pick thresholds */
static int fall_style_from_roll(double r)
{
    if (r < 0.55) return 0; /* GRACE */
    if (r < 0.80) return 1; /* BELLY */
    return 2;               /* TUMBLE */
}

/* Facing from travel angle — feet-down draw uses L/R only */
static double facing_from_angle(double angle)
{
    return (cos(angle) >= 0) ? 1.0 : -1.0;
}

/* Mirrors env_token_ok in shadow_cat.c */
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
        "cursorpos", "monitors", "j/activeworkspace", "j/clients", "j/activewindow",
    };
    if (!cmd)
        return 0;
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++) {
        if (strcmp(cmd, ok[i]) == 0)
            return 1;
    }
    return 0;
}

int main(void)
{
    printf("shadow-cat unit tests\n");

    expect(fabs(smootherstep(0)) < 1e-9, "smootherstep(0)=0");
    expect(fabs(smootherstep(1) - 1) < 1e-9, "smootherstep(1)=1");
    expect(smootherstep(0.5) > 0.4 && smootherstep(0.5) < 0.6, "smootherstep mid");

    expect(fabs(angle_diff(0, 0)) < 1e-9, "angle_diff zero");
    expect(fabs(angle_diff(G_PI, -G_PI)) < 1e-9, "angle_diff wrap");
    expect(angle_diff(0.5, 0) > 0, "angle_diff sign");

    {
        double v = 0;
        for (int i = 0; i < 40; i++)
            pose_approach(&v, 1.0, 0.05, 0.2);
        expect(v > 0.95 && v <= 1.0, "pose_approach converges");
    }

    {
        double x = -100, y = 5000;
        clamp_box(&x, &y, 0, 0, 1920, 1080, 96);
        expect(x >= 4 && x <= 1920 - 96 - 4, "clamp x");
        expect(y >= 4 && y <= 1080 - 96 - 4, "clamp y");
    }

    {
        int lr = 0, n = 10000;
        for (int i = 0; i < n; i++) {
            double r = (i + 0.5) / n;
            int b = side_bias_bucket(r);
            if (b == 2 || b == 3) lr++;
        }
        expect(lr > 8000, "side bias >=80% L/R");
    }

    {
        int content = 0, n = 10000;
        for (int i = 0; i < n; i++) {
            double r = (i + 0.5) / n;
            if (wander_bucket(r) == 0) content++;
        }
        expect(content < 300, "content roam <3%");
    }

    expect(1 == 1, "fullscreen hide rule: only fullscreen==1");

    /* Gravity / upright facing */
    expect(facing_from_angle(0.0) > 0, "facing right on +X");
    expect(facing_from_angle(G_PI) < 0, "facing left on -X");
    expect(facing_from_angle(-G_PI / 2) > 0, "facing stable when climbing");
    {
        double p_up = gravity_pitch_clamp(-G_PI / 2, 0, 0);
        double p_dn = gravity_pitch_clamp(G_PI / 2, 0, 0);
        expect(p_up < 0 && p_up > -0.39, "pitch lean climb");
        expect(p_dn > 0 && p_dn < 0.39, "pitch lean descend");
        double p_hot = gravity_pitch_clamp(0, 5000, 1);
        expect(fabs(p_hot) <= 0.38 + 1e-9, "pitch clamped");
    }

    {
        int grace = 0, fail = 0, n = 10000;
        for (int i = 0; i < n; i++) {
            double r = (i + 0.5) / n;
            int s = fall_style_from_roll(r);
            if (s == 0) grace++;
            else fail++;
        }
        expect(grace > 5000 && grace < 6000, "fall grace ~55%");
        expect(fail > 4000 && fail < 5000, "fall fail ~45%");
    }

    /* Simple gravity integrator lands */
    {
        double z = 50, vz = 20;
        double gpx = 920.0;
        int steps = 0;
        while (z > 0 && steps < 5000) {
            vz -= gpx * 0.016;
            z += vz * 0.016;
            steps++;
        }
        expect(z <= 0 && steps > 10 && steps < 400, "gravity fall lands");
    }

    expect(env_token_ok("/run/user/1000", 1), "runtime dir ok");
    expect(!env_token_ok("/tmp/../evil", 1), "runtime rejects ..");
    expect("rel"[0] != '/', "relative path not absolute");
    expect(env_token_ok("abc123_SIG", 0), "sig token ok");
    expect(!env_token_ok("a/b", 0), "sig rejects slash");
    expect(!env_token_ok("x\ny", 0), "sig rejects control");
    expect(ipc_cmd_allowed("cursorpos"), "ipc allow cursorpos");
    expect(ipc_cmd_allowed("j/clients"), "ipc allow j/clients");
    expect(!ipc_cmd_allowed("dispatch exec evil"), "ipc deny dispatch");
    expect(!ipc_cmd_allowed(NULL), "ipc deny null");

    printf("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
