#ifndef SCENES_H
#define SCENES_H

#include "physics.h"

typedef int (*SceneInitFn)(World* w, int cols, int rows, float user_restitution);

typedef struct {
    const char* name;
    const char* description;
    SceneInitFn init;
} Scene;

const Scene* scene_find(const char* name);
const Scene* scene_list(int* count);

#endif
