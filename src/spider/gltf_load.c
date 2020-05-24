#include "gltf_load.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "impl.h"
#include "debug.h"
#include "state.h"

extern _SPState _sp_state;

SPObject spLoadGltf(const char* file) {
    SPObject object = {};
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, file, &data);
    
    if(result == cgltf_result_success) {
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: parsed file\n", file);
        object.mesh = _spLoadMeshFromGltf(data, file);
        if(object.mesh.id) {
            object.material = _spLoadMaterialFromGltf(data, file);
        }
        cgltf_free(data);
    }
    else {
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: error %d\n", file, result);
    }
    if(!object.mesh.id || !object.material.id) {
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: error -> mesh: %d, material: %d\n", file, object.mesh.id, object.material.id);
    }
    return object;
}



SPMeshID _spLoadMeshFromGltf(cgltf_data* data, const char* gltf_path) {
    SPMeshID mesh_id = {0};
    cgltf_options options = {};
    if(data->meshes_count > 0) {
        cgltf_result buffers_result = cgltf_load_buffers(&options, data, gltf_path);
        if(buffers_result == cgltf_result_success) {
            DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: loaded buffers\n", gltf_path);
            const cgltf_mesh* mesh = &data->meshes[0];
            const cgltf_primitive* prim = &mesh->primitives[0];

            if(prim->attributes_count > 0) {
                uint16_t vertex_count = prim->attributes[0].data->count;
                uint16_t index_count = prim->indices->count;
                DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: found %d vertices and %d indices\n", gltf_path, vertex_count, index_count);
                SPVertex* vertex_data = SPIDER_MALLOC(sizeof (SPVertex) * vertex_count);
                uint16_t* index_data = SPIDER_MALLOC(sizeof (uint16_t) * index_count);
                memcpy(index_data, &prim->indices->buffer_view->buffer->data[prim->indices->buffer_view->offset], sizeof (uint16_t) * index_count);
                for(uint32_t attr_index = 0; attr_index < prim->attributes_count; attr_index++) {
                    const cgltf_attribute* attr = &prim->attributes[attr_index];
                    const uint8_t* attr_data = attr->data->buffer_view->buffer->data;
                    cgltf_size attr_offset = attr->data->buffer_view->offset;
                    switch(attr->type) {
                        case cgltf_attribute_type_position: {
                            for(uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                                memcpy(&vertex_data[vertex_index].pos, (vec3*)&attr_data[attr_offset + vertex_index * sizeof(vec3)], sizeof(vec3));
                            }
                            break;
                        }
                        case cgltf_attribute_type_normal: {
                            for(uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                                memcpy(&vertex_data[vertex_index].normal, (vec3*)&attr_data[attr_offset + vertex_index * sizeof(vec3)], sizeof(vec3));
                            }
                            break;
                        }
                        case cgltf_attribute_type_tangent: {
                            uint32_t w_negative_count = 0;
                            for(uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                                vec4* tangent = (vec4*)&attr_data[attr_offset + vertex_index * sizeof(vec4)];
                                if((*tangent)[3] == -1.0f) {
                                    w_negative_count++;
                                    // invert x component
                                    (*tangent)[0] = -(*tangent)[0];
                                    // swap y and z components
                                    float z = (*tangent)[2];
                                    (*tangent)[2] = (*tangent)[1];
                                    (*tangent)[1] = z;
                                }
                                memcpy(&vertex_data[vertex_index].tangent, (vec3*)tangent, sizeof(vec3));
                            }
                            DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: found %d tangents with w == -1.0\n", gltf_path, w_negative_count);
                            break;
                        }
                        case cgltf_attribute_type_texcoord: {
                            for(uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                                memcpy(&vertex_data[vertex_index].tex_coords, (vec2*)&attr_data[attr_offset + vertex_index * sizeof(vec2)], sizeof(vec2));
                            }
                            break;
                        }
                        default:
                        break;
                    }
                }
                
                SPMeshDesc mesh_desc = {
                    .vertices = {
                        .count = vertex_count,
                        .data = vertex_data
                    },
                    .indices = {
                        .count = index_count,
                        .data = index_data
                    }
                };
                mesh_id = spCreateMesh(&mesh_desc);
                free(vertex_data);
                free(index_data);
                DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: created mesh\n", gltf_path);
            }
        }
    }
    return mesh_id;
}

SPMaterialID _spLoadMaterialFromGltf(const cgltf_data* data, const char* gltf_path) {
    SPMaterialID mat_id = {0};
    if(data->meshes_count > 0) {
        const cgltf_mesh* mesh = &data->meshes[0];
        if(mesh->primitives_count > 0) { 
            const cgltf_primitive* prim = &mesh->primitives[0];
            const cgltf_material* mat = prim->material;
            char albedo[100] = {0};
            char* albedo_ptr = NULL;
            char normal[100] = {0};
            char* normal_ptr = NULL;
            char ao_roughness_metallic[100] = {0};
            char* ao_roughness_metallic_ptr = NULL;
            if(mat->has_pbr_metallic_roughness) {
                if(mat->pbr_metallic_roughness.base_color_texture.texture) {
                    _spModifyRelativeFilePath(gltf_path, mat->pbr_metallic_roughness.base_color_texture.texture->image->uri, albedo);
                    albedo_ptr = albedo;
                    DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: albedo texture -> %s\n", gltf_path, albedo);
                }
                if(mat->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                    _spModifyRelativeFilePath(gltf_path, mat->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri, ao_roughness_metallic);
                    ao_roughness_metallic_ptr = ao_roughness_metallic;
                    DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: ao_roughness_metallic texture -> %s\n", gltf_path, ao_roughness_metallic);
                }
            }
            if(mat->normal_texture.texture) {
                _spModifyRelativeFilePath(gltf_path, mat->normal_texture.texture->image->uri, normal);
                normal_ptr = normal;
                DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: normal texture -> %s\n", gltf_path, normal);
            }
            SPMaterialDesc mat_desc = {
                .albedo = albedo_ptr,
                .normal = normal_ptr,
                .ao_roughness_metallic = ao_roughness_metallic_ptr,
            };
            mat_id = spCreateMaterial(&mat_desc);
            DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: created material\n", gltf_path);
        }
    }
    return mat_id;
}

void _spModifyRelativeFilePath(const char* base_path, const char* new_path, char* result) {
    strcpy(result, base_path);
    char* insert_point = strrchr(result, '/');
    if(insert_point) {
        insert_point++;
    }
    else {
        insert_point = result;
    }
    strcpy(insert_point, new_path);
}