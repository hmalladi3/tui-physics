#ifndef PHYSICS_H
#define PHYSICS_H

#define MAX_BODIES 16

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    Vec2  pos;
    Vec2  vel;
    float radius;
    float restitution;
} Body;

typedef struct {
    Body  bodies[MAX_BODIES];
    int   n_bodies;
    float gravity;
    Vec2  bounds_min;
    Vec2  bounds_max;
} World;

World world_create(float width, float height, float gravity);
Body* world_add_body(World* w, Vec2 pos, Vec2 vel, float radius, float restitution);
void  world_step(World* w, float dt);
float world_total_kinetic_energy(const World* w);

#endif
