#ifndef SPIDER_INSTANCE_H_
#define SPIDER_INSTANCE_H_

#include <cglm/cglm.h>
#include "mesh.h"
#include "material.h"

typedef struct SPObject {
    SPMeshID mesh;
    SPMaterialID material;
} SPObject;

typedef struct SPTransform {
    vec3 pos; // 12 bytes
    vec3 rot; // 12 bytes
    vec3 scale; // 12 bytes
} SPTransform; // 36 bytes

typedef struct SPInstance {
    SPObject object;
    SPTransform transform;
} SPInstance;

typedef struct SPInstanceDesc {
    SPObject object;
    const SPTransform* transform;
} SPInstanceDesc;

typedef struct SPInstanceID {
    uint32_t id;
} SPInstanceID;

/*
Creates an instance and returns an identifier to it
*/
SPInstanceID spCreateInstance(const SPInstanceDesc* desc);

#endif // SPIDER_INSTANCE_H_