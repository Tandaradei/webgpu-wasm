#include "mesh.h"

#include "impl.h"
#include "debug.h"
#include "state.h"

extern _SPState _sp_state;

SPMeshID spCreateMesh(const SPMeshDesc* desc) {
    SPMeshID mesh_id = (SPMeshID){_spAllocPoolIndex(&(_sp_state.pools.mesh_pool))};
    if(mesh_id.id == SP_INVALID_ID) {
        return mesh_id;
    }
    int id = mesh_id.id;
    SPMesh* mesh = &(_sp_state.pools.meshes[id]);
    // SPVertex buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Vertex,
            .size = sizeof(SPVertex) * desc->vertices.count
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);

        memcpy(result.data, desc->vertices.data, result.dataLength);
        mesh->vertex_buffer = result.buffer;
        wgpuBufferUnmap(mesh->vertex_buffer);
    }

    // Index buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Index,
            .size = sizeof(uint16_t) * desc->indices.count
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);

        memcpy(result.data, desc->indices.data, result.dataLength);
        mesh->index_buffer = result.buffer;
        wgpuBufferUnmap(mesh->index_buffer);

        mesh->indices_count = desc->indices.count;
    }
    return mesh_id;
}

SPMeshID spCreateMeshFromInit(const SPMeshInitializer* init) {
    typedef struct Element {
        uint16_t v;
        uint16_t t;
    } Element;
    
    vec3* normals = calloc(init->vertices.count, sizeof *normals);
    
    Element* elements = calloc(init->faces.count * 3, sizeof *elements);
    uint16_t vertex_count = 0;
    uint16_t* indices = SPIDER_MALLOC(sizeof *indices * init->faces.count * 3);
    uint32_t index_count = 0;

    for(uint32_t f = 0; f < init->faces.count; f++) {
        vec3 face_normal = {0.0f, 0.0f, 0.0f};
        vec3 v01, v02;
        glm_vec3_sub(
            init->vertices.data[init->faces.data[f].vertex_indices[1]],
            init->vertices.data[init->faces.data[f].vertex_indices[0]],
            v01
        );
        glm_vec3_sub(
            init->vertices.data[init->faces.data[f].vertex_indices[2]],
            init->vertices.data[init->faces.data[f].vertex_indices[0]],
            v02
        );
        glm_cross(v01, v02, face_normal);
        glm_vec3_normalize(face_normal);
        DEBUG_PRINT(DEBUG_PRINT_MESH, "face %2d -> n: %f, %f, %f\n", f, face_normal[0], face_normal[1], face_normal[2]);
        for(uint8_t i = 0; i < 3; i++) {
            uint16_t v = init->faces.data[f].vertex_indices[i];
            uint16_t t = init->faces.data[f].tex_coord_indices[i];

            float d01 = glm_vec3_distance2(init->vertices.data[v], init->vertices.data[init->faces.data[f].vertex_indices[(i + 1) % 3]]);
            float d02 = glm_vec3_distance2(init->vertices.data[v], init->vertices.data[init->faces.data[f].vertex_indices[(i + 2) % 3]]);
            float max_distance = fmax(d01, d02);
            float scale_factor = 1.0f / max_distance;
            vec3 normal_add = {0.0f, 0.0f, 0.0f};
            glm_vec3_scale(face_normal, scale_factor, normal_add);
            glm_vec3_add(normals[v], normal_add, normals[v]);
            uint32_t e = 0;
            for(; e < vertex_count; e++) {
                if(elements[e].v == v && elements[e].t == t) {
                    break;
                }
            }
            if(e == vertex_count) {
                elements[e].v = v;
                elements[e].t = t;
                vertex_count++;
            }
            indices[index_count++] = e;
        }
    }

    for(uint16_t v = 0; v < init->vertices.count; v++) {
        glm_vec3_normalize(normals[v]);
    }
    
    SPVertex* vertices = SPIDER_MALLOC(sizeof *vertices * init->faces.count * 3);
    for(uint16_t i = 0; i < vertex_count; i++) {
        memcpy(vertices[i].pos, init->vertices.data[elements[i].v], sizeof vertices[i].pos);
        memcpy(vertices[i].tex_coords, init->tex_coords.data[elements[i].t], sizeof vertices[i].tex_coords);
        memcpy(vertices[i].normal, normals[elements[i].v], sizeof vertices[i].normal);
        memcpy(vertices[i].tangent , &(vec3){0.0f, 0.0f, 0.0f}, sizeof vertices[i].tangent);
    }

    for(uint32_t i = 0; i < index_count; i+= 3) {
        SPVertex* v0 = &(vertices[indices[i + 0]]);
        SPVertex* v1 = &(vertices[indices[i + 1]]);
        SPVertex* v2 = &(vertices[indices[i + 2]]);

        vec3 e01, e02;
        glm_vec3_sub(v1->pos, v0->pos, e01);
        glm_vec3_sub(v2->pos, v0->pos, e02);
        DEBUG_PRINT(DEBUG_PRINT_MESH, "e01: %.2f, %.2f, %.2f | e02: %.2f, %.2f, %.2f\n",
            e01[0], e01[1], e01[2],
            e02[0], e02[1], e02[2]
        );
        float deltaU1 = v1->tex_coords[0] - v0->tex_coords[0];
        float deltaV1 = v1->tex_coords[1] - v0->tex_coords[1];
        float deltaU2 = v2->tex_coords[0] - v0->tex_coords[0];
        float deltaV2 = v2->tex_coords[1] - v0->tex_coords[1];
        DEBUG_PRINT(DEBUG_PRINT_MESH, "d1: %.2f, %.2f | d2: %.2f, %.2f\n",
            deltaU1, deltaV1,
            deltaU2, deltaV2
        );

        float div = (deltaU1 * deltaV2 - deltaU2 * deltaU1);
        float f = div == 0.0f ? -1.0f : 1.0f / div;
        DEBUG_PRINT(DEBUG_PRINT_MESH, "f: %.2f\n", f);

        vec3 tangent = {
            f * (deltaV2 * e01[0] - deltaV1 * e02[0]),
            f * (deltaV2 * e01[1] - deltaV1 * e02[1]),
            f * (deltaV2 * e01[2] - deltaV1 * e02[2])
        };

        vec3 bitangent = {
            f * (-deltaU2 * e01[0] - deltaU1 * e02[0]),
            f * (-deltaU2 * e01[1] - deltaU1 * e02[1]),
            f * (-deltaU2 * e01[2] - deltaU1 * e02[2])
        };

        glm_vec3_add(v0->tangent, tangent, v0->tangent);
        glm_vec3_add(v1->tangent, tangent, v1->tangent);
        glm_vec3_add(v2->tangent, tangent, v2->tangent);

    }

    for(uint16_t i = 0; i < vertex_count; i++) {
        glm_vec3_normalize(vertices[i].tangent);
        DEBUG_PRINT(DEBUG_PRINT_MESH, "vertex %u -> t: %.2f, %.2f, %.2f\n", i, 
            vertices[i].tangent[0], vertices[i].tangent[1], vertices[i].tangent[2]
        );
    }

    SPMeshID mesh_id = spCreateMesh(&(SPMeshDesc){
        .vertices = {
            .data = vertices,
            .count = vertex_count,
        },
        .indices = {
            .data = indices,
            .count = index_count,
        }
    });

    DEBUG_PRINT(DEBUG_PRINT_MESH, "mesh create: created mesh with %d vertices and %d indices\n", vertex_count, index_count);

    free(elements);
    free(normals);
    free(vertices);
    free(indices);
    return mesh_id;
}