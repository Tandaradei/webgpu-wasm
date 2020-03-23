#ifndef VERTEX_H_
#define VERTEX_H_

#include <cglm/cglm.h>

typedef struct UBOCommon {
    mat4 view;
    mat4 proj;
} UBOCommon;

typedef struct UBODynamic {
    mat4 model;
} UBODynamic;

typedef struct Vertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
} Vertex;

#endif // VERTEX_H_