#ifndef SPIDER_H_
#define SPIDER_H_

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <webgpu/webgpu.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "helper.h"
#include "debug.h"
#include "vertex.h"

#define SPIDER_ASSERT(assertion) assert(assertion)
#define SPIDER_MALLOC(size) malloc(size)

typedef struct SPShaderStage {
    WGPUShaderModule module;
    WGPUBindGroupLayout bind_group_layout;
} SPShaderStage;

typedef struct SPCamera {
    vec3 pos;
    vec3 dir;
    float fovy;
    float aspect;
    float near;
    float far;
    mat4 _view;
    mat4 _proj;
} SPCamera;

#define SP_INVALID_ID (0)

typedef struct _SPPool {
    size_t size;
    size_t queue_top;
    uint32_t* gen_ctrs;
    int* free_queue;
} _SPPool;

typedef struct SPPoolsDesc {
    const struct {
        size_t materials;
        size_t meshes;
        size_t render_objects;
    } capacities;
} SPPoolsDesc;

typedef struct SPInitDesc {
    const struct {
        size_t width;
        size_t height;
    } surface_size;

    const SPCamera camera;

    SPPoolsDesc pools;
} SPInitDesc;

typedef struct SPMesh {
    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    size_t indices_count;
} SPMesh;

typedef struct SPMeshDesc {
    struct {
        const Vertex* data;
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

typedef struct SPMaterial {
    SPShaderStage vert;
    SPShaderStage frag;
    
    WGPUBuffer uniform_staging_buffer;
    WGPUBuffer uniform_buffer;
    WGPUBindGroup bind_group;

    WGPURenderPipeline pipeline;
} SPMaterial;

typedef struct SPMaterialDesc {
    const struct {
        const char* file;
    } vert;
    const struct {
        const char* file;
    } frag;
    const struct {
        const WGPUBindGroupLayoutBinding* data;
        const size_t count;
    } bgls;
    /*
    struct {
        const WGPUBindGroupBinding* data;
        const size_t count;
    } bindings;
    */
   const size_t uniform_buffer_size;
} SPMaterialDesc;

typedef struct SPMaterialID {
    uint32_t id;
} SPMaterialID;

typedef struct SPTransform {
    vec3 pos; // 12 bytes
    vec3 rot; // 12 bytes
    vec3 scale; // 12 bytes
} SPTransform; // 36 bytes

typedef struct _SPWriteUBO {
    UniformBufferObject ubo;
    SPMaterial* mat;
} _SPWriteUBO;

typedef struct SPRenderObject {
    SPMeshID mesh;
    SPMaterialID material;
    SPTransform transform;
    _SPWriteUBO ubo;
} SPRenderObject;

typedef struct SPRenderObjectDesc {
    SPMeshID mesh;
    SPMaterialID material;
} SPRenderObjectDesc;

typedef struct SPRenderObjectID {
    uint32_t id;
} SPRenderObjectID;

typedef struct _SPPools{
    _SPPool material_pool;
    SPMaterial* materials;
    _SPPool mesh_pool;
    SPMesh* meshes;
    _SPPool render_object_pool;
    SPRenderObject* render_objects;
} _SPPools;

#define _SP_GET_DEFAULT_IF_ZERO(value, default_value) value ? value : default_value 

#define _SP_MATERIAL_POOL_MAX 8
#define _SP_MESH_POOL_MAX 256
#define _SP_RENDER_OBJECT_POOL_MAX 256

struct {
    WGPUDevice device;
    WGPUQueue queue;

    WGPUInstance instance;
    WGPUSurface surface;
    
    WGPUSwapChain swap_chain;
    WGPUCommandEncoder cmd_enc;
    WGPURenderPassEncoder pass_enc;

    _SPPools pools;
    SPCamera active_cam;
} _sp_state;

void errorCallback(WGPUErrorType type, char const * message, void * userdata) {
    printf("%d: %s\n", type, message);
}

// PUBLIC
void spInit(const SPInitDesc* desc);
void spUpdateTransforms();
void spRender(void);
SPMeshID spCreateMesh(const SPMeshDesc* desc);
SPMaterialID spCreateMaterial(const SPMaterialDesc* desc);
SPRenderObjectID spCreateRenderObject(const SPRenderObjectDesc* desc);

// PRIVATE
void _spInitPool(_SPPool* pool, size_t size) {
    SPIDER_ASSERT(pool && size >= 1);
    pool->size = size + 1;
    pool->queue_top = 0;
    size_t gen_ctrs_size = sizeof(uint32_t) * pool->size;
    pool->gen_ctrs = (uint32_t*) SPIDER_MALLOC(gen_ctrs_size);
    SPIDER_ASSERT(pool->gen_ctrs);
    pool->free_queue = (int*)SPIDER_MALLOC(sizeof(int)*size);
    SPIDER_ASSERT(pool->free_queue);
    /* never allocate the zero-th pool item since the invalid id is 0 */
    for (int i = pool->size-1; i >= 1; i--) {
        pool->free_queue[pool->queue_top++] = i;
    }
}

void _spSetupPools(_SPPools* pools, const SPPoolsDesc* pools_desc) {
    _spInitPool(&(_sp_state.pools.mesh_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.meshes, _SP_MESH_POOL_MAX));
    size_t mesh_pool_byte_size = sizeof(SPMesh) * pools->mesh_pool.size;
    pools->meshes = (SPMesh*) SPIDER_MALLOC(mesh_pool_byte_size);
    SPIDER_ASSERT(pools->meshes);
    memset(pools->meshes, 0, mesh_pool_byte_size);

    
    _spInitPool(&(_sp_state.pools.material_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.materials, _SP_MATERIAL_POOL_MAX));
    size_t mat_pool_byte_size = sizeof(SPMesh) * pools->material_pool.size;
    pools->materials = (SPMaterial*) SPIDER_MALLOC(mat_pool_byte_size);
    SPIDER_ASSERT(pools->materials);
    memset(pools->materials, 0, mat_pool_byte_size);

    _spInitPool(&(_sp_state.pools.render_object_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.render_objects, _SP_RENDER_OBJECT_POOL_MAX));
    size_t ren_obj_pool_byte_size = sizeof(SPMesh) * pools->render_object_pool.size;
    pools->render_objects = (SPRenderObject*) SPIDER_MALLOC(ren_obj_pool_byte_size);
    SPIDER_ASSERT(pools->render_objects);
    memset(pools->render_objects, 0, ren_obj_pool_byte_size);
}

void _spDiscardPool(_SPPool* pool) {
    SPIDER_ASSERT(pool);
    SPIDER_ASSERT(pool->free_queue);
    SPIDER_ASSERT(pool->free_queue);
    pool->free_queue = 0;
    SPIDER_ASSERT(pool->gen_ctrs);
    SPIDER_ASSERT(pool->gen_ctrs);
    pool->gen_ctrs = 0;
    pool->size = 0;
    pool->queue_top = 0;
}

int _spAllocPoolIndex(_SPPool* pool) {
    SPIDER_ASSERT(pool);
    SPIDER_ASSERT(pool->free_queue);
    if (pool->queue_top > 0) {
        int slot_index = pool->free_queue[--pool->queue_top];
        SPIDER_ASSERT((slot_index > 0) && (slot_index < pool->size));
        return slot_index;
    }
    else {
        /* pool exhausted */
        return SP_INVALID_ID;
    }
}

void _spFreePoolIndex(_SPPool* pool, int slot_index) {
    SPIDER_ASSERT((slot_index > SP_INVALID_ID) && (slot_index < pool->size));
    SPIDER_ASSERT(pool);
    SPIDER_ASSERT(pool->free_queue);
    SPIDER_ASSERT(pool->queue_top < pool->size);
    #ifdef SPIDER_DEBUG
    /* debug check against double-free */
    for (int i = 0; i < pool->queue_top; i++) {
        SPIDER_ASSERT(pool->free_queue[i] != slot_index);
    }
    #endif
    pool->free_queue[pool->queue_top++] = slot_index;
    SPIDER_ASSERT(pool->queue_top <= (pool->size-1));
}

// IMPLEMENTATION
void spInit(const SPInitDesc* desc) {
    _sp_state.device = emscripten_webgpu_get_device();
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: got device\n");
    wgpuDeviceSetUncapturedErrorCallback(_sp_state.device, &errorCallback, NULL);

    _sp_state.queue = wgpuDeviceCreateQueue(_sp_state.device);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created queue\n");

    WGPUSurfaceDescriptorFromHTMLCanvasId canvas_desc = {
        .nextInChain = NULL,
        .sType = WGPUSType_SurfaceDescriptorFromHTMLCanvasId,
        .id = "canvas"
    };

    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = (const WGPUChainedStruct*)&canvas_desc
    };

    _sp_state.instance = NULL;  // null instance
    _sp_state.surface = wgpuInstanceCreateSurface(_sp_state.instance, &surf_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created surface\n");

    WGPUSwapChainDescriptor sc_desc = {
        .usage = WGPUTextureUsage_OutputAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = desc->surface_size.width,
        .height = desc->surface_size.height,
        .presentMode = WGPUPresentMode_VSync
    };
    _sp_state.swap_chain = wgpuDeviceCreateSwapChain(_sp_state.device, _sp_state.surface, &sc_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created swapChain\n");

    _sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
    
    _spSetupPools(&(_sp_state.pools), &(desc->pools));
}

void _spWriteMVPCallback(WGPUBufferMapAsyncStatus status, void * data, uint64_t dataLength, void * userdata) {
    SPIDER_ASSERT(status == WGPUBufferMapAsyncStatus_Success);

    _SPWriteUBO* block = (_SPWriteUBO*)(userdata);
    memcpy(data, &(block->ubo), sizeof(UniformBufferObject));
    wgpuBufferUnmap(block->mat->uniform_staging_buffer);

    WGPUCommandBuffer commands;
    wgpuCommandEncoderCopyBufferToBuffer(_sp_state.cmd_enc, block->mat->uniform_staging_buffer, 0, block->mat->uniform_buffer, 0, sizeof(UniformBufferObject));
}

void _spUpdateTransform(SPRenderObject* ren_obj) {
    mat4 model = GLM_MAT4_IDENTITY_INIT;
    //glm_rotate(model.model, );
    //glm_scale();
    glm_translate(model, ren_obj->transform.pos);
    mat4 model_view = GLM_MAT4_IDENTITY_INIT;
    glm_mat4_mul(_sp_state.active_cam._view, model, model_view);
    SPMaterialID mat_id = ren_obj->material;
    SPMaterial* material = &(_sp_state.pools.materials[mat_id.id]);
    ren_obj->ubo = (_SPWriteUBO){
        .mat = material
    };
    memcpy(ren_obj->ubo.ubo.model_view, model_view, sizeof(mat4));
    memcpy(ren_obj->ubo.ubo.proj, _sp_state.active_cam._proj, sizeof(mat4));
    wgpuBufferMapWriteAsync(material->uniform_staging_buffer, _spWriteMVPCallback, &ren_obj->ubo);
}

void _spUpdateView(void) {
    vec3 up = { 0.0f, 1.0f, 0.0f };
    glm_look(
        _sp_state.active_cam.pos,
        _sp_state.active_cam.dir,
        up,
        _sp_state.active_cam._view
    );
}

void _spUpdateProjection(void) {
    glm_perspective(
        _sp_state.active_cam.fovy,
        _sp_state.active_cam.aspect,
        _sp_state.active_cam.near,
        _sp_state.active_cam.far,
        _sp_state.active_cam._proj
    );
}

void _spUpdate(void) {
    _spUpdateView();
    _spUpdateProjection();
}

void spRender(void) {
    _spUpdate();
    WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);
    
    WGPURenderPassColorAttachmentDescriptor attachment = {
        .attachment = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearColor = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f}
    };
    //printf("render: created renderPassColorAttachmentDescriptor\n");

    WGPURenderPassDescriptor render_pass = {
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment
    };
    
    SPMaterialID last_mat_id = {0};
    for(size_t i = 1; i < _sp_state.pools.render_object_pool.size; i++) {
        SPMeshID mesh_id = _sp_state.pools.render_objects[i].mesh;
        SPMaterialID mat_id = _sp_state.pools.render_objects[i].material;
        if(!mesh_id.id || !mat_id.id) {
            continue;
        }
        _spUpdateTransform(&(_sp_state.pools.render_objects[i]));
        SPMesh* mesh =  &(_sp_state.pools.meshes[mesh_id.id]);
        SPMaterial* material = &(_sp_state.pools.materials[mat_id.id]);
        
        _sp_state.pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
        wgpuRenderPassEncoderSetPipeline(_sp_state.pass_enc, material->pipeline);
        wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 0, material->bind_group, 0, NULL);
        wgpuRenderPassEncoderSetVertexBuffer(_sp_state.pass_enc, 0, mesh->vertex_buffer, 0);
        wgpuRenderPassEncoderSetIndexBuffer(_sp_state.pass_enc, mesh->index_buffer, 0);
        wgpuRenderPassEncoderDrawIndexed(_sp_state.pass_enc, mesh->indices_count, 1, 0, 0, 0);
        wgpuRenderPassEncoderEndPass(_sp_state.pass_enc);

    }
    
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(_sp_state.cmd_enc, NULL);
    wgpuCommandEncoderRelease(_sp_state.cmd_enc);
    _sp_state.cmd_enc = NULL;
    wgpuQueueSubmit(_sp_state.queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    _sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
}

SPMeshID spCreateMesh(const SPMeshDesc* desc) {
    SPMeshID mesh_id = (SPMeshID){_spAllocPoolIndex(&(_sp_state.pools.mesh_pool))};
    if(mesh_id.id == SP_INVALID_ID) {
        return mesh_id;
    }
    int id = mesh_id.id;
    SPMesh* mesh = &(_sp_state.pools.meshes[id]);
    // Vertex buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Vertex,
            .size = sizeof(Vertex) * desc->vertices.count
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

SPMaterialID spCreateMaterial(const SPMaterialDesc* desc) {
    SPMaterialID material_id = (SPMaterialID){_spAllocPoolIndex(&(_sp_state.pools.material_pool))};
    if(material_id.id == SP_INVALID_ID) {
        return material_id;
    }
    int id = material_id.id; 
    SPMaterial* material = &(_sp_state.pools.materials[id]);
    {
        FileReadResult vertShader;
        readFile(desc->vert.file, &vertShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", vertShader.size);
        WGPUShaderModuleDescriptor sm_desc = {
            .codeSize = vertShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)vertShader.data
        };
        material->vert.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
    }
    {
        FileReadResult fragShader;
        readFile(desc->frag.file, &fragShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", fragShader.size);
        WGPUShaderModuleDescriptor sm_desc = {
            .codeSize = fragShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)fragShader.data
        };
        material->frag.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
    }
    if(desc->bgls.count > 0 && desc->uniform_buffer_size > 0) {
        // Uniform buffer creation
        {
            WGPUBufferDescriptor buffer_desc = {
                .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                .size = sizeof(UniformBufferObject)
            };

            material->uniform_buffer = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
        }

        // Uniform staging buffer creation
        {
            WGPUBufferDescriptor buffer_desc = {
                .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc,
                .size = sizeof(UniformBufferObject)
            };

            WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);

            memset(result.data, 0, result.dataLength);
            material->uniform_staging_buffer = result.buffer;
            wgpuBufferUnmap(material->uniform_staging_buffer);
        }

        WGPUBindGroupLayoutDescriptor bgl_desc = {
            .bindingCount = desc->bgls.count,
            .bindings = desc->bgls.data
        };
        material->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &bgl_desc);
        material->frag.bind_group_layout = NULL;

        WGPUBindGroupBinding bindings[] = {
            {
                .binding = 0,
                .buffer = material->uniform_buffer,
                .offset = 0,
                .size = sizeof(UniformBufferObject),
                .sampler = NULL,
                .textureView = NULL,
            }
        };

        WGPUBindGroupDescriptor bg_desc = {
            .layout = material->vert.bind_group_layout,
            .bindingCount = ARRAY_LEN(bindings),
            .bindings = bindings
        };
        material->bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &bg_desc);
    }
    else {
        WGPUBindGroupLayoutDescriptor bgl_desc = {
            .bindingCount = 0,
            .bindings = NULL
        };
        material->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &bgl_desc);
        WGPUBindGroupDescriptor bg_desc = {
            .layout = material->vert.bind_group_layout,
            .bindingCount = 0,
            .bindings = NULL
        };
        material->bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &bg_desc);
    }

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &(material->vert.bind_group_layout)
    };

    WGPUProgrammableStageDescriptor frag_stage_desc = {
        .module = material->frag.module,
        .entryPoint = "main"
    };

    WGPUColorStateDescriptor color_state_desc = {
        .format = WGPUTextureFormat_BGRA8Unorm,
        .alphaBlend = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_Zero,
        },
        .colorBlend = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_Zero,
        },
        .writeMask = WGPUColorWriteMask_All
    };

    WGPUVertexAttributeDescriptor vertex_attribute_descs[] = {
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(Vertex, pos),
            .shaderLocation = 0
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(Vertex, color),
            .shaderLocation = 1
        }
    };

    WGPUVertexBufferLayoutDescriptor vert_buffer_layout_descs[] = {
        {
            .arrayStride = sizeof(Vertex),
            .stepMode = WGPUInputStepMode_Vertex,
            .attributeCount = ARRAY_LEN(vertex_attribute_descs),
            .attributes = vertex_attribute_descs
        }
    };

    WGPUVertexStateDescriptor vert_state_desc = {
        .indexFormat = WGPUIndexFormat_Uint16,
        .vertexBufferCount = ARRAY_LEN(vert_buffer_layout_descs),
        .vertexBuffers = vert_buffer_layout_descs
    };

    WGPURasterizationStateDescriptor rast_state_desc = {
        .nextInChain = NULL,
        .frontFace = WGPUFrontFace_CCW,
        .cullMode = WGPUCullMode_None,
        .depthBias = 0,
        .depthBiasSlopeScale = 0.0f,
        .depthBiasClamp = 0.0f,
    };

    WGPURenderPipelineDescriptor pipeline_desc = {
        .layout = wgpuDeviceCreatePipelineLayout(_sp_state.device, &pipeline_layout_desc),
        .vertexStage.module = material->vert.module,
        .vertexStage.entryPoint = "main",
        .vertexState = &vert_state_desc,
        .fragmentStage = &frag_stage_desc,
        .rasterizationState = &rast_state_desc,
        .colorStateCount = 1,
        .colorStates = &color_state_desc,
        .primitiveTopology = WGPUPrimitiveTopology_TriangleList,
        .sampleCount = 1,
        .sampleMask = 0xFFFFFFFF
    };
    material->pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
    return material_id;
}

SPRenderObjectID spCreateRenderObject(const SPRenderObjectDesc* desc) {
    SPIDER_ASSERT(desc->mesh.id > 0 && desc->material.id > 0);
    if(!desc->mesh.id || !desc->material.id) {
        return (SPRenderObjectID){0};
    }
    SPRenderObjectID render_object_id = (SPRenderObjectID){_spAllocPoolIndex(&(_sp_state.pools.render_object_pool))};
    if(render_object_id.id == SP_INVALID_ID) {
        return render_object_id;
    }
    int id = render_object_id.id; 
    SPRenderObject* ren_obj = &(_sp_state.pools.render_objects[id]);
    ren_obj->mesh = desc->mesh;
    ren_obj->material = desc->material;
    ren_obj->transform = (SPTransform){
        .scale = {1.0f, 1.0f, 1.0f},
    };
    return render_object_id;
}

#endif // SPIDER_H_