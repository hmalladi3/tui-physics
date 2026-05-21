#include "physics.h"
#include "render.h"
#include "scenes.h"
#include "term.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const float DEFAULT_GRAVITY     = 80.0f;
static const float DEFAULT_RESTITUTION = 0.85f;

typedef struct {
    float         gravity;
    float         restitution;
    const char*   scene_name;
    unsigned long bench_steps;   /* 0 = interactive mode */
} Args;

static void print_usage(FILE* out) {
    fputs(
        "usage: physics [--gravity F] [--restitution F] [--scene NAME] [--bench N] [-h | --help]\n"
        "  --gravity F       gravity in dots/s^2 (default 80.0; may be negative)\n"
        "  --restitution F   bounce energy retained, 0..1 (default 0.85)\n"
        "  --scene NAME      scene to run (default: \"default\")\n"
        "  --bench N         run N physics steps headlessly, print throughput, exit\n"
        "  -h, --help        show this message and exit\n"
        "\nScenes:\n",
        out);
    int          n;
    const Scene* list = scene_list(&n);
    for (int i = 0; i < n; i++) {
        fprintf(out, "  %-10s %s\n", list[i].name, list[i].description);
    }
}

/* * Returns 0 on success, 1 if --help printed (caller exits 0), -1 on error (caller exits 2). */
static int parse_args(int argc, char** argv, Args* out) {
    out->gravity     = DEFAULT_GRAVITY;
    out->restitution = DEFAULT_RESTITUTION;
    out->scene_name  = "default";
    out->bench_steps = 0;

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            print_usage(stdout);
            return 1;
        }
        int is_gravity     = (strcmp(a, "--gravity")     == 0);
        int is_restitution = (strcmp(a, "--restitution") == 0);
        int is_scene       = (strcmp(a, "--scene")       == 0);
        int is_bench       = (strcmp(a, "--bench")       == 0);
        if (!is_gravity && !is_restitution && !is_scene && !is_bench) {
            fprintf(stderr, "physics: unknown option '%s' (try --help)\n", a);
            return -1;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "physics: missing value for %s\n", a);
            return -1;
        }
        const char* v = argv[++i];
        if (is_scene) {
            if (!scene_find(v)) {
                fprintf(stderr, "physics: unknown scene '%s' (try --help)\n", v);
                return -1;
            }
            out->scene_name = v;
            continue;
        }
        if (is_bench) {
            char*         end;
            unsigned long n = strtoul(v, &end, 10);
            if (end == v || *end != '\0' || n == 0) {
                fprintf(stderr, "physics: --bench must be a positive integer (got '%s')\n", v);
                return -1;
            }
            out->bench_steps = n;
            continue;
        }
        char* end;
        float f = strtof(v, &end);
        if (end == v || *end != '\0') {
            fprintf(stderr, "physics: '%s' is not a number (for %s)\n", v, a);
            return -1;
        }
        if (!isfinite(f)) {
            fprintf(stderr, "physics: %s must be finite (got '%s')\n", a, v);
            return -1;
        }
        if (is_gravity) {
            out->gravity = f;
        } else {
            if (f < 0.0f || f > 1.0f) {
                fprintf(stderr, "physics: --restitution must be in [0, 1] (got %g)\n", (double)f);
                return -1;
            }
            out->restitution = f;
        }
    }
    return 0;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void sleep_until(double deadline) {
    double remaining = deadline - now_seconds();
    if (remaining <= 0.0) return;
    if (remaining < 0.0015) {
        while (now_seconds() < deadline) { /* spin */ }
        return;
    }
    double          sleep_for = remaining - 0.001;
    time_t          secs      = (time_t)sleep_for;
    struct timespec req       = {secs, (long)((sleep_for - (double)secs) * 1e9)};
    nanosleep(&req, NULL);
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int run_bench(const Args* args) {
    const int   BENCH_COLS = 80;
    const int   BENCH_ROWS = 24;
    const float PHYS_DT    = 1.0f / 240.0f;

    World world       = world_create((float)BENCH_COLS * 2.0f, (float)BENCH_ROWS * 4.0f, args->gravity);
    world.bounds_min.y = 4.0f;

    const Scene* scene = scene_find(args->scene_name);
    if (!scene || scene->init(&world, BENCH_COLS, BENCH_ROWS, args->restitution) != 0) {
        fprintf(stderr, "physics: scene '%s' failed to initialize for bench\n", args->scene_name);
        return 1;
    }

    printf("scene: %s, bodies: %d\n", args->scene_name, world.n_bodies);
    fflush(stdout);

    /* Warm-up: 1000 steps before timer starts so the first iterations don't pay * cache-miss / TLB-fault costs that aren't representative of steady-state. */
    for (int i = 0; i < 1000; i++) {
        world_step(&world, PHYS_DT);
    }

    double start = now_seconds();
    for (unsigned long i = 0; i < args->bench_steps; i++) {
        world_step(&world, PHYS_DT);
    }
    double elapsed = now_seconds() - start;

    double sps         = (double)args->bench_steps / elapsed;
    double us_per_step = elapsed * 1e6 / (double)args->bench_steps;

    printf("%lu steps in %.3f s\n", args->bench_steps, elapsed);
    if (sps >= 1e6) {
        printf("%.2fM steps/sec\n", sps / 1e6);
    } else if (sps >= 1e3) {
        printf("%.2fK steps/sec\n", sps / 1e3);
    } else {
        printf("%.2f steps/sec\n", sps);
    }
    printf("%.2f us/step\n", us_per_step);
    return 0;
}

int main(int argc, char** argv) {
    Args args;
    int  rc = parse_args(argc, argv, &args);
    if (rc == 1) return 0;
    if (rc < 0) return 2;

    if (args.bench_steps > 0) return run_bench(&args);

    if (term_init() != 0) return 1;

    int cols = 0, rows = 0;
    term_get_size(&cols, &rows);

    if (render_init(cols, rows) != 0) return 1;

    World world = world_create((float)cols * 2.0f, (float)rows * 4.0f, args.gravity);
    world.bounds_min.y = 4.0f; /* reserve top cell row for stats overlay */

    const Scene* scene = scene_find(args.scene_name);
    /* parse_args validated scene_name, so this should never be NULL. */
    if (!scene || scene->init(&world, cols, rows, args.restitution) != 0) {
        fprintf(stderr, "physics: scene '%s' failed to initialize (terminal too small?)\n",
                args.scene_name);
        return 1;
    }

    const float  PHYS_DT       = 1.0f / 240.0f;
    const double RENDER_PERIOD = 1.0  / 60.0;
    const float  MAX_FRAME     = 0.25f;
    const double STATS_PERIOD  = 0.25;

    double last_time        = now_seconds();
    double next_render      = last_time;
    float  accumulator      = 0.0f;
    double stats_last_time  = last_time;
    int    stats_frame_count = 0;
    char   stats_buf[128];

    srand((unsigned int)time(NULL));

    while (!term_quit_requested()) {
        Event ev;
        if (term_poll_event(&ev)) {
            if (ev.type == EVENT_KEY && ev.key == 'q') break;
            if (ev.type == EVENT_MOUSE_CLICK && ev.mouse.button == 0) {
                /* Convert 1-indexed cell coords to mid-cell dot coords. */
                float dx = (float)(ev.mouse.col - 1) * 2.0f + 1.0f;
                float dy = (float)(ev.mouse.row - 1) * 4.0f + 2.0f;
                Vec2  vel = {(float)(rand() % 41 - 20), (float)(rand() % 41 - 20)};
                world_add_body(&world, (Vec2){dx, dy}, vel, 4.0f, args.restitution);
            }
        }

        if (term_resize_pending()) {
            term_get_size(&cols, &rows);
            render_resize(cols, rows);
            world.bounds_max = (Vec2){(float)cols * 2.0f, (float)rows * 4.0f};
            for (int i = 0; i < world.n_bodies; i++) {
                Body* b = &world.bodies[i];
                b->pos.x = clampf(b->pos.x, world.bounds_min.x + b->radius,
                                  world.bounds_max.x - b->radius);
                b->pos.y = clampf(b->pos.y, world.bounds_min.y + b->radius,
                                  world.bounds_max.y - b->radius);
            }
        }

        double current = now_seconds();
        float  frame   = (float)(current - last_time);
        if (frame > MAX_FRAME) frame = MAX_FRAME;
        last_time = current;
        accumulator += frame;

        while (accumulator >= PHYS_DT) {
            world_step(&world, PHYS_DT);
            accumulator -= PHYS_DT;
        }

        if (current >= next_render) {
            render_clear();
            for (int i = 0; i < world.n_bodies; i++) {
                Body* b = &world.bodies[i];
                render_circle(b->pos.x, b->pos.y, b->radius);
            }
            render_present();
            next_render += RENDER_PERIOD;
            if (current - next_render > RENDER_PERIOD) {
                next_render = current + RENDER_PERIOD;
            }

            stats_frame_count++;
            double stats_elapsed = current - stats_last_time;
            if (stats_elapsed >= STATS_PERIOD) {
                float fps = (float)((double)stats_frame_count / stats_elapsed);
                float ke  = world_total_kinetic_energy(&world);
                snprintf(stats_buf, sizeof stats_buf,
                         "FPS %5.1f  bodies %d/%d  KE %.1f",
                         (double)fps, world.n_bodies, MAX_BODIES, (double)ke);
                render_stats(stats_buf);
                stats_last_time  = current;
                stats_frame_count = 0;
            }
        }

        double next_phys = last_time + (double)(PHYS_DT - accumulator);
        double deadline  = next_render < next_phys ? next_render : next_phys;
        sleep_until(deadline);
    }

    return 0;
}
