#ifndef SPIDER_MESH_H_
#define SPIDER_MESH_H_

#include <webgpu/webgpu.h>
#include <cglm/cglm.h>

typedef struct SPVertex {
    vec3 pos;
    vec2 tex_coords;
    vec3 normal;
    vec3 tangent;
} SPVertex;

typedef struct SPTriangle {
    uint16_t vertex_indices[3];
    uint16_t tex_coord_indices[3];
} SPTriangle;

typedef struct SPMeshInitializerDesc {
    struct {
        vec3* data;
        uint16_t count;
    } vertices;
    struct {
        vec2* data;
        uint16_t count;
    } tex_coords;
    struct {
        SPTriangle* data;
        uint32_t count;
    } faces;
} SPMeshInitializerDesc;

typedef struct SPMesh {
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    size_t indices_count;
} SPMesh;

typedef struct SPMeshDesc {
    struct {
        const SPVertex* data;
        const size_t count;
    } vertices;
    struct {
        const uint16_t* data;
        const size_t count;
    } indices;
} SPMeshDesc;

typedef struct SPMeshID {
    uint32_t id;
} SPMeshID;

/*
Creates a mesh and returns an identifier to it
*/
SPMeshID spCreateMesh(const SPMeshDesc* desc);
// TODO: add description
SPMeshID spCreateMeshFromInit(const SPMeshInitializerDesc* desc);

#endif // SPIDER_MESH_H_