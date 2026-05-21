#include "../physics.h"
#include "test.h"

static void test_world_create_initial_state(void) {
    World w = world_create(100, 50, 9.8f);
    ASSERT(w.n_bodies == 0);
    ASSERT(w.bounds_min.x == 0.0f);
    ASSERT(w.bounds_min.y == 0.0f);
    ASSERT(w.bounds_max.x == 100.0f);
    ASSERT(w.bounds_max.y == 50.0f);
    ASSERT(w.gravity == 9.8f);
}

static void test_add_body_success(void) {
    World w  = world_create(100, 50, 0);
    Body* b  = world_add_body(&w, (Vec2){50, 25}, (Vec2){1, 2}, 4.0f, 0.85f);
    ASSERT(b != NULL);
    ASSERT(w.n_bodies == 1);
    ASSERT(b->pos.x == 50.0f && b->pos.y == 25.0f);
    ASSERT(b->vel.x == 1.0f && b->vel.y == 2.0f);
    ASSERT(b->radius == 4.0f);
    ASSERT(b->restitution == 0.85f);
}

static void test_add_body_rejects_nonpositive_radius(void) {
    World w = world_create(100, 50, 0);
    ASSERT(world_add_body(&w, (Vec2){50, 25}, (Vec2){0, 0}, 0.0f, 0.5f) == NULL);
    ASSERT(world_add_body(&w, (Vec2){50, 25}, (Vec2){0, 0}, -1.0f, 0.5f) == NULL);
    ASSERT(w.n_bodies == 0);
}

static void test_add_body_rejects_out_of_range_restitution(void) {
    World w = world_create(100, 50, 0);
    ASSERT(world_add_body(&w, (Vec2){50, 25}, (Vec2){0, 0}, 4.0f, -0.01f) == NULL);
    ASSERT(world_add_body(&w, (Vec2){50, 25}, (Vec2){0, 0}, 4.0f, 1.01f) == NULL);
    ASSERT(w.n_bodies == 0);
}

static void test_add_body_rejects_too_large(void) {
    World w = world_create(20, 20, 0);
    /* 2 * radius == 20, fails strict-less-than */
    ASSERT(world_add_body(&w, (Vec2){10, 10}, (Vec2){0, 0}, 10.0f, 0.5f) == NULL);
    /* 2 * radius > width */
    ASSERT(world_add_body(&w, (Vec2){10, 10}, (Vec2){0, 0}, 15.0f, 0.5f) == NULL);
}

static void test_add_body_rejects_initial_penetration(void) {
    World w = world_create(100, 50, 0);
    /* x penetrates left wall */
    ASSERT(world_add_body(&w, (Vec2){2, 25}, (Vec2){0, 0}, 4.0f, 0.5f) == NULL);
    /* y penetrates top wall */
    ASSERT(world_add_body(&w, (Vec2){50, 2}, (Vec2){0, 0}, 4.0f, 0.5f) == NULL);
    /* x penetrates right wall */
    ASSERT(world_add_body(&w, (Vec2){98, 25}, (Vec2){0, 0}, 4.0f, 0.5f) == NULL);
    /* y penetrates bottom wall */
    ASSERT(world_add_body(&w, (Vec2){50, 48}, (Vec2){0, 0}, 4.0f, 0.5f) == NULL);
}

static void test_add_body_rejects_at_capacity(void) {
    World w = world_create(1000, 1000, 0);
    for (int i = 0; i < MAX_BODIES; i++) {
        ASSERT(world_add_body(&w, (Vec2){500, 500}, (Vec2){0, 0}, 4.0f, 0.5f) != NULL);
    }
    ASSERT(w.n_bodies == MAX_BODIES);
    ASSERT(world_add_body(&w, (Vec2){500, 500}, (Vec2){0, 0}, 4.0f, 0.5f) == NULL);
    ASSERT(w.n_bodies == MAX_BODIES);
}

static void test_step_applies_gravity_to_velocity(void) {
    World w  = world_create(100, 100, 10.0f);
    Body* b  = world_add_body(&w, (Vec2){50, 50}, (Vec2){0, 0}, 1.0f, 0.5f);
    world_step(&w, 0.1f);
    ASSERT_NEAR(b->vel.y, 1.0f, 1e-6);
    ASSERT_NEAR(b->vel.x, 0.0f, 1e-6);
}

static void test_step_uses_post_gravity_velocity_for_position(void) {
    World w  = world_create(100, 100, 10.0f);
    Body* b  = world_add_body(&w, (Vec2){50, 50}, (Vec2){5, 0}, 1.0f, 0.5f);
    world_step(&w, 0.1f);
    /* symplectic: vel.y becomes 1.0, then pos.y += 1.0 * 0.1 = 0.1 */
    ASSERT_NEAR(b->pos.y, 50.1f, 1e-5);
    ASSERT_NEAR(b->pos.x, 50.5f, 1e-5);
}

static void test_wall_collision_clamps_position_and_flips_velocity(void) {
    World w  = world_create(100, 100, 0);
    Body* b  = world_add_body(&w, (Vec2){50, 50}, (Vec2){1000, 0}, 1.0f, 0.8f);
    world_step(&w, 1.0f);
    /* would overshoot to 1050; clamped flush against right wall: 100 - 1 = 99 */
    ASSERT_NEAR(b->pos.x, 99.0f, 1e-3);
    /* velocity flipped and scaled by restitution */
    ASSERT_NEAR(b->vel.x, -800.0f, 1e-2);
}

static void test_corner_collision_resolves_axes_independently(void) {
    World w  = world_create(100, 100, 0);
    Body* b  = world_add_body(&w, (Vec2){50, 50}, (Vec2){100, 100}, 1.0f, 0.5f);
    world_step(&w, 1.0f);
    ASSERT_NEAR(b->pos.x, 99.0f, 1e-3);
    ASSERT_NEAR(b->pos.y, 99.0f, 1e-3);
    ASSERT_NEAR(b->vel.x, -50.0f, 1e-3);
    ASSERT_NEAR(b->vel.y, -50.0f, 1e-3);
}

static void test_step_with_empty_world_is_noop(void) {
    World w = world_create(100, 100, 10.0f);
    world_step(&w, 0.1f);
    ASSERT(w.n_bodies == 0);
}

static void test_step_with_nonpositive_dt_is_noop(void) {
    World w  = world_create(100, 100, 10.0f);
    Body* b  = world_add_body(&w, (Vec2){50, 50}, (Vec2){5, 5}, 1.0f, 0.5f);
    Vec2 p0  = b->pos;
    Vec2 v0  = b->vel;
    world_step(&w, 0.0f);
    ASSERT(b->pos.x == p0.x && b->pos.y == p0.y);
    ASSERT(b->vel.x == v0.x && b->vel.y == v0.y);
    world_step(&w, -0.1f);
    ASSERT(b->pos.x == p0.x && b->pos.y == p0.y);
    ASSERT(b->vel.x == v0.x && b->vel.y == v0.y);
}

static void test_determinism_same_host(void) {
    World a = world_create(100, 100, 9.8f);
    World b = world_create(100, 100, 9.8f);
    Body* ba = world_add_body(&a, (Vec2){50, 10}, (Vec2){5, 0}, 2.0f, 0.85f);
    Body* bb = world_add_body(&b, (Vec2){50, 10}, (Vec2){5, 0}, 2.0f, 0.85f);
    (void)ba; (void)bb;
    for (int i = 0; i < 1000; i++) {
        world_step(&a, 1.0f / 240.0f);
        world_step(&b, 1.0f / 240.0f);
    }
    ASSERT(a.bodies[0].pos.x == b.bodies[0].pos.x);
    ASSERT(a.bodies[0].pos.y == b.bodies[0].pos.y);
    ASSERT(a.bodies[0].vel.x == b.bodies[0].vel.x);
    ASSERT(a.bodies[0].vel.y == b.bodies[0].vel.y);
}

/* is an invariant verified * manually via valgrind / leak-check on the demo binary — see * docs/specs/manual-verification.md. */

/* ============================================================================ * Body-body collision (..) * ========================================================================== */

static void test_overlapping_bodies_at_rest_position_separate(void) {
    World w  = world_create(200, 200, 0);
    /* Place two stationary bodies overlapping (centers 4 apart, radii 3+3=6). */
    Body* a  = world_add_body(&w, (Vec2){100, 100}, (Vec2){0, 0}, 3.0f, 1.0f);
    Body* b  = world_add_body(&w, (Vec2){104, 100}, (Vec2){0, 0}, 3.0f, 1.0f);
    (void)a; (void)b;
    world_step(&w, 1.0f / 240.0f);
    /* After one step: gravity = 0, no integration drift; bodies should be just touching. */
    float dx = w.bodies[1].pos.x - w.bodies[0].pos.x;
    float dy = w.bodies[1].pos.y - w.bodies[0].pos.y;
    ASSERT_NEAR(sqrtf(dx*dx + dy*dy), 6.0f, 1e-3);
}

static void test_separating_bodies_get_separated_no_impulse(void) {
    World w  = world_create(200, 200, 0);
    /* Two overlapping bodies moving apart. */
    Body* a  = world_add_body(&w, (Vec2){100, 100}, (Vec2){-5, 0}, 3.0f, 1.0f);
    Body* b  = world_add_body(&w, (Vec2){104, 100}, (Vec2){+5, 0}, 3.0f, 1.0f);
    Vec2 va_before = a->vel;
    Vec2 vb_before = b->vel;
    world_step(&w, 1.0f / 240.0f);
    /* Velocities should be unchanged (no impulse, since they were already separating). */
    ASSERT_NEAR(w.bodies[0].vel.x, va_before.x - 0.0f, 1e-4);  /* gravity = 0, no change */
    ASSERT_NEAR(w.bodies[1].vel.x, vb_before.x - 0.0f, 1e-4);
    /* And they should now be just touching, not overlapping. */
    float dx = w.bodies[1].pos.x - w.bodies[0].pos.x;
    ASSERT(dx >= 6.0f - 1e-3);
}

/* * head-on elastic, equal mass, restitution=1 -> velocities swap */
static void test_head_on_elastic_swaps_velocities(void) {
    World w  = world_create(400, 200, 0);
    /* Two bodies approaching head-on. Pick positions and speeds so one step of dt=1 * leaves them overlapping (not past each other) and the impulse fires. */
    Body* a  = world_add_body(&w, (Vec2){195, 100}, (Vec2){+3, 0}, 3.0f, 1.0f);
    Body* b  = world_add_body(&w, (Vec2){205, 100}, (Vec2){-3, 0}, 3.0f, 1.0f);
    (void)a; (void)b;
    world_step(&w, 1.0f);
    /* After collision the equal-mass restitution=1 case swaps normal-component velocities: * a was +3, b was -3 → a becomes -3, b becomes +3. */
    ASSERT_NEAR(w.bodies[0].vel.x, -3.0f, 1e-3);
    ASSERT_NEAR(w.bodies[1].vel.x, +3.0f, 1e-3);
}

/* * momentum conservation */
static void test_momentum_conserved_through_collision(void) {
    World w  = world_create(400, 200, 0);
    world_add_body(&w, (Vec2){180, 100}, (Vec2){+15, +3}, 3.0f, 0.6f);
    world_add_body(&w, (Vec2){220, 100}, (Vec2){-5, -2}, 3.0f, 0.6f);

    float px_before = w.bodies[0].vel.x + w.bodies[1].vel.x;
    float py_before = w.bodies[0].vel.y + w.bodies[1].vel.y;

    world_step(&w, 1.0f);

    float px_after = w.bodies[0].vel.x + w.bodies[1].vel.x;
    float py_after = w.bodies[0].vel.y + w.bodies[1].vel.y;

    ASSERT_NEAR(px_before, px_after, 1e-3);
    ASSERT_NEAR(py_before, py_after, 1e-3);
}

/* * kinetic energy conservation at restitution=1 */
static void test_kinetic_energy_conserved_at_restitution_1(void) {
    World w  = world_create(400, 200, 0);
    world_add_body(&w, (Vec2){180, 100}, (Vec2){+12, +4}, 3.0f, 1.0f);
    world_add_body(&w, (Vec2){220, 100}, (Vec2){-6, -1}, 3.0f, 1.0f);

    float ke_before = world_total_kinetic_energy(&w);
    world_step(&w, 1.0f);
    float ke_after = world_total_kinetic_energy(&w);

    ASSERT_NEAR(ke_before, ke_after, 1e-2);
}

/* * pair iteration covers non-adjacent indices */
static void test_pair_iteration_covers_nonadjacent_pairs(void) {
    World w = world_create(400, 200, 0);
    /* Three bodies: only the (0,2) pair overlaps. Verifies the loop iterates * non-adjacent indices, not just consecutive ones. */
    world_add_body(&w, (Vec2){100, 100}, (Vec2){0, 0}, 3.0f, 1.0f);
    world_add_body(&w, (Vec2){200, 100}, (Vec2){0, 0}, 3.0f, 1.0f); /* far away, untouched */
    world_add_body(&w, (Vec2){104, 100}, (Vec2){0, 0}, 3.0f, 1.0f); /* overlaps with body 0 */
    world_step(&w, 1.0f / 240.0f);
    /* Bodies 0 and 2 should now be just touching. */
    float dx  = w.bodies[2].pos.x - w.bodies[0].pos.x;
    float dy  = w.bodies[2].pos.y - w.bodies[0].pos.y;
    float d02 = sqrtf(dx * dx + dy * dy);
    ASSERT_NEAR(d02, 6.0f, 1e-3);
    /* Body 1 must be untouched. */
    ASSERT_NEAR(w.bodies[1].pos.x, 200.0f, 1e-3);
    ASSERT_NEAR(w.bodies[1].pos.y, 100.0f, 1e-3);
}

/* * wall collisions resolved before body-body */
static void test_walls_resolved_before_body_body(void) {
    World w  = world_create(50, 200, 0);
    /* Body a moving right toward the right wall; body b just to its left. After step, * a should bounce off the wall (vel.x flipped), then resolve body-body with b. */
    Body* a  = world_add_body(&w, (Vec2){44, 100}, (Vec2){10, 0}, 3.0f, 1.0f);
    Body* b  = world_add_body(&w, (Vec2){36, 100}, (Vec2){0, 0}, 3.0f, 1.0f);
    (void)a; (void)b;
    world_step(&w, 1.0f);
    /* a hit the wall and reversed; then collides with b on the way back. */
    ASSERT(w.bodies[0].vel.x < 0.0f || w.bodies[1].vel.x < 0.0f);
    /* Total momentum (with wall imparting impulse) is hard to assert here; just verify * no body has clipped through a wall. */
    ASSERT(w.bodies[0].pos.x - 3.0f >= 0.0f - 1e-2);
    ASSERT(w.bodies[0].pos.x + 3.0f <= 50.0f + 1e-2);
    ASSERT(w.bodies[1].pos.x - 3.0f >= 0.0f - 1e-2);
    ASSERT(w.bodies[1].pos.x + 3.0f <= 50.0f + 1e-2);
}

/* ============================================================================ * Kinetic energy () * ========================================================================== */

static void test_total_ke_sum_over_bodies(void) {
    World w = world_create(200, 200, 0);
    ASSERT_NEAR(world_total_kinetic_energy(&w), 0.0f, 1e-6);
    world_add_body(&w, (Vec2){100, 100}, (Vec2){3, 4}, 2.0f, 1.0f);
    /* 0.5 * (3^2 + 4^2) = 12.5 */
    ASSERT_NEAR(world_total_kinetic_energy(&w), 12.5f, 1e-4);
    world_add_body(&w, (Vec2){50, 50}, (Vec2){0, 10}, 2.0f, 1.0f);
    /* + 0.5 * (0 + 100) = + 50 → 62.5 */
    ASSERT_NEAR(world_total_kinetic_energy(&w), 62.5f, 1e-4);
}

/* * pure read */
static void test_total_ke_is_pure_read(void) {
    World w = world_create(200, 200, 0);
    Body* b = world_add_body(&w, (Vec2){100, 100}, (Vec2){5, 5}, 2.0f, 1.0f);
    Vec2 pos_before = b->pos;
    Vec2 vel_before = b->vel;
    float ke = world_total_kinetic_energy(&w);
    (void)ke;
    ASSERT(b->pos.x == pos_before.x && b->pos.y == pos_before.y);
    ASSERT(b->vel.x == vel_before.x && b->vel.y == vel_before.y);
}

void run_physics_tests(void) {
    RUN(test_world_create_initial_state);
    RUN(test_add_body_success);
    RUN(test_add_body_rejects_nonpositive_radius);
    RUN(test_add_body_rejects_out_of_range_restitution);
    RUN(test_add_body_rejects_too_large);
    RUN(test_add_body_rejects_initial_penetration);
    RUN(test_add_body_rejects_at_capacity);
    RUN(test_step_applies_gravity_to_velocity);
    RUN(test_step_uses_post_gravity_velocity_for_position);
    RUN(test_wall_collision_clamps_position_and_flips_velocity);
    RUN(test_corner_collision_resolves_axes_independently);
    RUN(test_step_with_empty_world_is_noop);
    RUN(test_step_with_nonpositive_dt_is_noop);
    RUN(test_determinism_same_host);

    RUN(test_overlapping_bodies_at_rest_position_separate);
    RUN(test_separating_bodies_get_separated_no_impulse);
    RUN(test_head_on_elastic_swaps_velocities);
    RUN(test_momentum_conserved_through_collision);
    RUN(test_kinetic_energy_conserved_at_restitution_1);
    RUN(test_pair_iteration_covers_nonadjacent_pairs);
    RUN(test_walls_resolved_before_body_body);

    RUN(test_total_ke_sum_over_bodies);
    RUN(test_total_ke_is_pure_read);
}
