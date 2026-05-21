#include "../physics.h"
#include "../scenes.h"
#include "test.h"

#include <string.h>

static World make_world(int cols, int rows) {
    World w        = world_create((float)cols * 2.0f, (float)rows * 4.0f, 80.0f);
    w.bounds_min.y = 4.0f;
    return w;
}

static void test_find_returns_matching_entry(void) {
    const Scene* s = scene_find("default");
    ASSERT(s != NULL);
    ASSERT(strcmp(s->name, "default") == 0);
    ASSERT(s->init != NULL);
}

static void test_find_returns_null_for_unknown(void) {
    ASSERT(scene_find("foobar") == NULL);
    ASSERT(scene_find("") == NULL);
    ASSERT(scene_find(NULL) == NULL);
}

static void test_list_returns_all_four(void) {
    int n = 0;
    const Scene* list = scene_list(&n);
    ASSERT(n >= 4);
    int found_default  = 0, found_cradle = 0, found_diagonal = 0, found_stack = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].name, "default")  == 0) found_default  = 1;
        if (strcmp(list[i].name, "cradle")   == 0) found_cradle   = 1;
        if (strcmp(list[i].name, "diagonal") == 0) found_diagonal = 1;
        if (strcmp(list[i].name, "stack")    == 0) found_stack    = 1;
    }
    ASSERT(found_default && found_cradle && found_diagonal && found_stack);
}

static void test_default_spawns_one_body(void) {
    World w = make_world(40, 20);
    const Scene* s = scene_find("default");
    ASSERT(s->init(&w, 40, 20, 0.85f) == 0);
    ASSERT(w.n_bodies == 1);
    ASSERT(w.bodies[0].radius == 4.0f);
    ASSERT(w.bodies[0].restitution == 0.85f);
    ASSERT(w.bodies[0].vel.x == 30.0f);
}

static void test_cradle_spawns_five_bodies_zero_gravity(void) {
    World w = make_world(60, 30);
    const Scene* s = scene_find("cradle");
    ASSERT(s->init(&w, 60, 30, 0.5f) == 0);  /* restitution should be ignored */
    ASSERT(w.n_bodies == 5);
    ASSERT(w.gravity == 0.0f);
    for (int i = 0; i < 5; i++) {
        ASSERT(w.bodies[i].restitution == 1.0f);
    }
    /* Exactly one body has nonzero horizontal velocity (the incoming ball). */
    int n_moving = 0;
    for (int i = 0; i < 5; i++) {
        if (w.bodies[i].vel.x != 0.0f) n_moving++;
    }
    ASSERT(n_moving == 1);
}

static void test_cradle_refuses_narrow_terminal(void) {
    World w = make_world(20, 10);
    const Scene* s = scene_find("cradle");
    /* 20 cols * 2 = 40 dots. Cradle needs ~70 dots. Should refuse. */
    ASSERT(s->init(&w, 20, 10, 1.0f) == -1);
    /* World should not be partially populated. */
    ASSERT(w.n_bodies == 0);
}

static void test_diagonal_spawns_one_body_zero_gravity(void) {
    World w = make_world(40, 20);
    const Scene* s = scene_find("diagonal");
    ASSERT(s->init(&w, 40, 20, 0.5f) == 0);  /* restitution ignored */
    ASSERT(w.n_bodies == 1);
    ASSERT(w.gravity == 0.0f);
    ASSERT(w.bodies[0].restitution == 1.0f);
    ASSERT(w.bodies[0].vel.x != 0.0f);
    ASSERT(w.bodies[0].vel.y != 0.0f);
}

static void test_stack_spawns_four_bodies(void) {
    World w = make_world(40, 20);
    const Scene* s = scene_find("stack");
    ASSERT(s->init(&w, 40, 20, 0.7f) == 0);
    ASSERT(w.n_bodies == 4);
    for (int i = 0; i < 4; i++) {
        ASSERT(w.bodies[i].radius == 3.0f);
        ASSERT(w.bodies[i].restitution == 0.7f);
        ASSERT(w.bodies[i].vel.x == 0.0f);
        ASSERT(w.bodies[i].vel.y == 0.0f);
    }
    /* Bodies should be stacked vertically (same x, increasing y). */
    for (int i = 1; i < 4; i++) {
        ASSERT(w.bodies[i].pos.x == w.bodies[0].pos.x);
        ASSERT(w.bodies[i].pos.y > w.bodies[i - 1].pos.y);
    }
}

void run_scenes_tests(void) {
    RUN(test_find_returns_matching_entry);
    RUN(test_find_returns_null_for_unknown);
    RUN(test_list_returns_all_four);
    RUN(test_default_spawns_one_body);
    RUN(test_cradle_spawns_five_bodies_zero_gravity);
    RUN(test_cradle_refuses_narrow_terminal);
    RUN(test_diagonal_spawns_one_body_zero_gravity);
    RUN(test_stack_spawns_four_bodies);
}
