/*
 * Headless hot-path microbench — reference for perf bake-offs when
 * the Wayland display isn't reachable from the agent environment.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define G_PI 3.14159265358979323846
#define N 200000

static double angle_diff(double a, double b)
{
    return atan2(sin(a - b), cos(a - b));
}

static void pose_approach(double *v, double target, double dt, double tau)
{
    double k = 1.0 - exp(-dt / fmax(tau, 0.05));
    *v += (target - *v) * k;
}

static void steer(double *vx, double *vy, double wish_x, double wish_y,
                  double max_spd, double dt, double accel)
{
    double wlen = hypot(wish_x, wish_y);
    double tx = 0, ty = 0;
    if (wlen > 1e-6) {
        tx = (wish_x / wlen) * max_spd;
        ty = (wish_y / wlen) * max_spd;
    }
    *vx += (tx - *vx) * fmin(1.0, accel * dt);
    *vy += (ty - *vy) * fmin(1.0, accel * dt);
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(void)
{
    double sit = 0, fat = 0.2, lng = 0.5, sx = 1, sy = 1;
    double vx = 0, vy = 0, ang = 0, avis = 0;
    double x = 100, y = 100;
    int margin_calls = 0;
    int last_mx = -1, last_my = -1;

    double t0 = now_ms();
    for (int i = 0; i < N; i++) {
        double dt = 0.033;
        pose_approach(&sit, 2.4, dt, 0.18);
        pose_approach(&fat, 0.16, dt, 0.18);
        pose_approach(&lng, 0.48, dt, 0.18);
        pose_approach(&sx, 1.05, dt, 0.18);
        pose_approach(&sy, 0.9, dt, 0.18);
        steer(&vx, &vy, 1.0, 0.2, 130.0, dt, 5.5);
        x += vx * dt;
        y += vy * dt;
        ang = atan2(vy, vx);
        avis += angle_diff(ang, avis) * 0.22;
        int mx = (int)lround(x), my = (int)lround(y);
#ifdef BENCH_SKIP_REDUNDANT_MARGINS
        if (mx != last_mx || my != last_my) {
            margin_calls++;
            last_mx = mx;
            last_my = my;
        }
#else
        (void)last_mx; (void)last_my;
        margin_calls++;
#endif
#ifdef BENCH_SKIP_IDLE_POSE
        if (fabs(sit - 2.4) < 1e-3 && fabs(fat - 0.16) < 1e-3)
            continue; /* pretend early-out */
#endif
        (void)sx; (void)sy; (void)avis;
    }
    double t1 = now_ms();
    printf("iters=%d\n", N);
    printf("ms=%.3f\n", t1 - t0);
    printf("ns_per_iter=%.1f\n", (t1 - t0) * 1e6 / N);
    printf("margin_calls=%d\n", margin_calls);
    printf("final_xy=%.1f,%.1f\n", x, y);
    return 0;
}
