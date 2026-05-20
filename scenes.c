#include "scenes.h"

#include <stddef.h>
#include <string.h>

static int scene_default(World* w, int cols, int rows, float restitution) {
    Vec2 pos = {(float)cols, (float)rows};
    Vec2 vel = {30.0f, 0.0f};
    return world_add_body(w, pos, vel, 4.0f, restitution) ? 0 : -1;
}

static int scene_cradle(World* w, int cols, int rows, float restitution) {
    (void)restitution;
    w->gravity = 0.0f;
    float       r        = 4.0f;
    const int   n_static = 4;
    float       y        = (float)(rows * 4) - r - 4.0f;
    float       chain_x0 = (float)cols;
    float       gap      = 2.0f * r;
    float       incoming = chain_x0 - gap - 2.0f * r;

    /* Refuse if the cradle layout would not fit in the world. */
    if (incoming - r < w->bounds_min.x) return -1;
    if (chain_x0 + (n_static - 1) * 2.0f * r + r > w->bounds_max.x) return -1;
    if (y - r < w->bounds_min.y) return -1;

    for (int i = 0; i < n_static; i++) {
        Vec2 pos = {chain_x0 + (float)i * 2.0f * r, y};
        if (!world_add_body(w, pos, (Vec2){0, 0}, r, 1.0f)) return -1;
    }
    if (!world_add_body(w, (Vec2){incoming, y}, (Vec2){30.0f, 0.0f}, r, 1.0f)) return -1;
    return 0;
}

static int scene_diagonal(World* w, int cols, int rows, float restitution) {
    (void)restitution;
    w->gravity = 0.0f;
    Vec2 pos = {(float)cols, (float)(rows * 2)};
    Vec2 vel = {25.0f, 20.0f};
    return world_add_body(w, pos, vel, 4.0f, 1.0f) ? 0 : -1;
}

static int scene_stack(World* w, int cols, int rows, float restitution) {
    (void)rows;
    float     r       = 3.0f;
    const int n       = 4;
    float     spacing = 2.0f * r + 1.0f;
    float     x       = (float)cols;
    float     y_top   = w->bounds_min.y + r + 2.0f;

    if (y_top + (n - 1) * spacing + r > w->bounds_max.y) return -1;
    if (x - r < w->bounds_min.x || x + r > w->bounds_max.x) return -1;

    for (int i = 0; i < n; i++) {
        Vec2 pos = {x, y_top + (float)i * spacing};
        if (!world_add_body(w, pos, (Vec2){0, 0}, r, restitution)) return -1;
    }
    return 0;
}

static const Scene scene_table[] = {
    {"default",  "single ball under gravity (v1 demo)",                  scene_default},
    {"cradle",   "Newton's cradle: incoming ball into 4-ball chain",     scene_cradle},
    {"diagonal", "single ball, zero gravity, bouncing diagonally",       scene_diagonal},
    {"stack",    "vertical stack of balls falling under gravity",        scene_stack},
};

const Scene* scene_find(const char* name) {
    if (!name) return NULL;
    size_t n = sizeof scene_table / sizeof scene_table[0];
    for (size_t i = 0; i < n; i++) {
        if (strcmp(scene_table[i].name, name) == 0) return &scene_table[i];
    }
    return NULL;
}

const Scene* scene_list(int* count) {
    *count = (int)(sizeof scene_table / sizeof scene_table[0]);
    return scene_table;
}
