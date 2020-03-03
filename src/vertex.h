#ifndef VERTEX_H_
#define VERTEX_H_

#include <cglm/cglm.h>

typedef struct UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} UniformBufferObject;

typedef struct Vertex {
    vec3 pos;
    vec3 color;
} Vertex;

#endif // VERTEX_H_