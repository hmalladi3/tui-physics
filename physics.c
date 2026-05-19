#include "physics.h"

#include <math.h>
#include <stddef.h>

World world_create(float width, float height, float gravity) {
    World w = {0};
    w.gravity    = gravity;
    w.bounds_min = (Vec2){0.0f, 0.0f};
    w.bounds_max = (Vec2){width, height};
    return w;
}

Body* world_add_body(World* w, Vec2 pos, Vec2 vel, float radius, float restitution) {
    if (radius <= 0.0f) return NULL;
    if (restitution < 0.0f || restitution > 1.0f) return NULL;

    float width  = w->bounds_max.x - w->bounds_min.x;
    float height = w->bounds_max.y - w->bounds_min.y;
    if (2.0f * radius >= width || 2.0f * radius >= height) return NULL;

    if (pos.x - radius < w->bounds_min.x) return NULL;
    if (pos.x + radius > w->bounds_max.x) return NULL;
    if (pos.y - radius < w->bounds_min.y) return NULL;
    if (pos.y + radius > w->bounds_max.y) return NULL;

    if (w->n_bodies >= MAX_BODIES) return NULL;

    Body* b        = &w->bodies[w->n_bodies++];
    b->pos         = pos;
    b->vel         = vel;
    b->radius      = radius;
    b->restitution = restitution;
    return b;
}

void world_step(World* w, float dt) {
    if (dt <= 0.0f) return;
    if (w->n_bodies == 0) return;

    /* Pass 1: integrate + wall resolution per body. */
    for (int i = 0; i < w->n_bodies; i++) {
        Body* b = &w->bodies[i];

        b->vel.y += w->gravity * dt;
        b->pos.x += b->vel.x * dt;
        b->pos.y += b->vel.y * dt;

        if (b->pos.x - b->radius < w->bounds_min.x) {
            b->pos.x = w->bounds_min.x + b->radius;
            b->vel.x = -b->vel.x * b->restitution;
        } else if (b->pos.x + b->radius > w->bounds_max.x) {
            b->pos.x = w->bounds_max.x - b->radius;
            b->vel.x = -b->vel.x * b->restitution;
        }

        if (b->pos.y - b->radius < w->bounds_min.y) {
            b->pos.y = w->bounds_min.y + b->radius;
            b->vel.y = -b->vel.y * b->restitution;
        } else if (b->pos.y + b->radius > w->bounds_max.y) {
            b->pos.y = w->bounds_max.y - b->radius;
            b->vel.y = -b->vel.y * b->restitution;
        }
    }

    /* Pass 2: body-body collision, sequential impulse over all i<j pairs. */
    for (int i = 0; i < w->n_bodies; i++) {
        for (int j = i + 1; j < w->n_bodies; j++) {
            Body* a = &w->bodies[i];
            Body* b = &w->bodies[j];

            float dx     = b->pos.x - a->pos.x;
            float dy     = b->pos.y - a->pos.y;
            float dist2  = dx * dx + dy * dy;
            float sum_r  = a->radius + b->radius;
            float sumr2  = sum_r * sum_r;

            if (dist2 > sumr2) continue;
            if (dist2 == 0.0f) {
                /* Coincident centers: pick an arbitrary normal so we can separate. */
                dx    = 1.0f;
                dy    = 0.0f;
                dist2 = 1.0f;
            }

            float dist = sqrtf(dist2);
            float nx   = dx / dist;
            float ny   = dy / dist;

            /* position-separate by half the overlap each, along the normal. */
            float overlap = sum_r - dist;
            float half    = overlap * 0.5f;
            a->pos.x -= nx * half;
            a->pos.y -= ny * half;
            b->pos.x += nx * half;
            b->pos.y += ny * half;

            /* impulse only if bodies are approaching. */
            float v_rel_x = b->vel.x - a->vel.x;
            float v_rel_y = b->vel.y - a->vel.y;
            float v_rel_n = v_rel_x * nx + v_rel_y * ny;
            if (v_rel_n >= 0.0f) continue;

            float e = a->restitution < b->restitution ? a->restitution : b->restitution;
            float J = -(1.0f + e) * v_rel_n * 0.5f;
            a->vel.x -= J * nx;
            a->vel.y -= J * ny;
            b->vel.x += J * nx;
            b->vel.y += J * ny;
        }
    }
}

float world_total_kinetic_energy(const World* w) {
    float ke = 0.0f;
    for (int i = 0; i < w->n_bodies; i++) {
        const Body* b = &w->bodies[i];
        ke += b->vel.x * b->vel.x + b->vel.y * b->vel.y;
    }
    return 0.5f * ke;
}
