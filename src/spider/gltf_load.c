#include "gltf_load.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "impl.h"
#include "debug.h"
#include "state.h"

extern _SPState _sp_state;

SPSceneNodeID spLoadGltf(const char* file) {
    SPSceneNodeID root_id = {SP_INVALID_ID};
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, file, &data);
    
    if(result == cgltf_result_success) {
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: parsed file\n", file);
        
        cgltf_result buffers_result = cgltf_load_buffers(&options, data, file);
        
        uint32_t mesh_node_count = 0;
        uint32_t prim_node_count = 0;
        
        if(buffers_result == cgltf_result_success) {
            // cache materials - use material pointer as identifier
            uint32_t material_count = data->materials_count;
            SPMaterialID* material_map = SP_MALLOC(sizeof(SPMaterialID) * material_count);
            cgltf_material** mat_ptr_to_index = SP_MALLOC(sizeof(cgltf_material*) * material_count);

            for(uint32_t mat = 0; mat < material_count; mat++) {
                DEBUG_PRINT(DEBUG_PRINT_WARNING, "Load material %d\n", mat);
                cgltf_material* material =&data->materials[mat];
                mat_ptr_to_index[mat] = material;
                SPMaterialID mat_id = _spLoadMaterialFromGltf(material, file);
                SP_ASSERT(mat_id.id);
                material_map[mat] = mat_id;
            }


            if(data->nodes_count > 1) {
                root_id = spCreateEmptySceneNode(&(SPEmptySceneNodeDesc){
                    .transform = &(SPTransform) {
                        .pos = {0.0f, 0.0f, 0.0f},
                        .rot = {0.0f, 0.0f, 0.0f},
                        .scale = {1.0f, 1.0f, 1.0f}
                    },
                    .parent = {SP_INVALID_ID}
                });
            }

            for(uint32_t n = 0; n < data->nodes_count; n++) {
                const cgltf_node* node = &data->nodes[n];
                SPTransform transform = {
                    .pos = {0.0f, 0.0f, 0.0f},
                    .rot = {0.0f, 0.0f, 0.0f},
                    .scale = {1.0f, 1.0f, 1.0f},
                };
                if(node->has_translation) {
                    memcpy(transform.pos, node->translation, sizeof(vec3));
                }
                if(node->has_rotation) {
                    glm_quat_axis((float*)node->rotation, transform.rot);
                }
                if(node->has_scale) {
                    memcpy(transform.scale, node->scale, sizeof(vec3));
                }
                SPSceneNodeID node_id = spCreateEmptySceneNode(&(SPEmptySceneNodeDesc){
                    .transform = &transform,
                    .parent = root_id
                });
                if(root_id.id == SP_INVALID_ID) {
                    root_id = node_id;
                }

                const cgltf_mesh* mesh = node->mesh;
                spSceneNodeSetChildrenCapacity(spGetSceneNode(node_id), mesh->primitives_count);

                for(uint32_t p = 0; p < mesh->primitives_count; p++) {
                    const cgltf_primitive* prim = &mesh->primitives[p];

                    DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "Load primitive %d\n", p);
                    SPMeshID mesh_id = _spLoadMeshPrimitiveFromGltf(prim, file);
                    
                    DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "Created mesh %d\n", mesh_id.id);
                    // search for material in the list of pre-loaded materials
                    uint32_t mat = 0;
                    while(mat < material_count && mat_ptr_to_index[mat] != prim->material) {
                        mat++;
                    }
                    if(mat < material_count) {
                        SPMaterialID material_id = material_map[mat];
                        SP_ASSERT(mesh_id.id && material_id.id);
                        if(mesh_id.id != SP_INVALID_ID && material_id.id != SP_INVALID_ID) {
                            spCreateRenderMeshSceneNode(&(SPRenderMeshSceneNodeDesc){
                                .mesh = mesh_id,
                                .material = material_id,
                                .transform = &(SPTransform) {
                                    .pos = {0.0f, 0.0f, 0.0f},
                                    .rot = {0.0f, 0.0f, 0.0f},
                                    .scale = {1.0f, 1.0f, 1.0f}
                                },
                                .parent = node_id
                            });
                            prim_node_count++;
                        }
                    }
                }
            }
            free(material_map);
            free(mat_ptr_to_index);
        }
        cgltf_free(data);
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: Created %d (Empty) mesh nodes with %d (RenderMesh) primitive nodes\n", file, mesh_node_count, prim_node_count);
    }
    else {
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: error %d\n", file, result);
    }
    return root_id;
}

SPMeshID _spLoadMeshPrimitiveFromGltf(const cgltf_primitive* prim, const char* gltf_path) {
    SP_ASSERT(prim && gltf_path);
    SPMeshID mesh_id = {SP_INVALID_ID};

    if(prim->attributes_count > 0) {
        uint16_t vertex_count = prim->attributes[0].data->count;
        uint16_t index_count = prim->indices->count;
        DEBUG_PRINT(DEBUG_PRINT_GLTF_LOAD, "%s: found %d vertices and %d indices\n", gltf_path, vertex_count, index_count);
        SPVertex* vertex_data = SP_MALLOC(sizeof (SPVertex) * vertex_count);
        uint16_t* index_data = SP_MALLOC(sizeof (uint16_t) * index_count);
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
                    for(uint32_t vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
                        vec4* tangent = (vec4*)&attr_data[attr_offset + vertex_index * sizeof(vec4)];
                        if((*tangent)[3] == -1.0f) {
                            // invert x component
                            (*tangent)[0] = -(*tangent)[0];
                            // swap y and z components
                            float z = (*tangent)[2];
                            (*tangent)[2] = (*tangent)[1];
                            (*tangent)[1] = z;
                        }
                        memcpy(&vertex_data[vertex_index].tangent, (vec3*)tangent, sizeof(vec3));
                    }
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
    return mesh_id;
}

SPMaterialID _spLoadMaterialFromGltf(const cgltf_material* mat, const char* gltf_path) {
    SP_ASSERT(mat && gltf_path);
    SPMaterialID mat_id = {0};
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