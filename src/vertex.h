#ifndef VERTEX_H_
#define VERTEX_H_

#include <cglm/cglm.h>

typedef struct UBOCommon {
    mat4 view;
    mat4 proj;
    vec3 light_pos;
} UBOCommon;

typedef struct MaterialProperties {
    float specular;
} MaterialProperties;

typedef struct UBODynamic {
    mat4 model;
    MaterialProperties material;
} UBODynamic;

typedef struct Vertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
} Vertex;

#endif // VERTEX_H_