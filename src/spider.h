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
#include "vertex.h"

#define SPIDER_ASSERT(assertion) assert(assertion)
#define SPIDER_MALLOC(size) malloc(size)

#define DEBUG_PRINT_TYPE_INIT 1
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 1
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 1
#define DEBUG_PRINT_RENDER 0
#define DEBUG_PRINT_METRICS 1

typedef struct SPShaderStage {
    WGPUShaderModule module;
    WGPUBindGroupLayout bind_group_layout;
} SPShaderStage;

typedef enum SPCameraMode {
    SPCameraMode_Direction,
    SPCameraMode_LookAt
} SPCameraMode;

typedef struct SPCamera {
    vec3 pos;
    vec3 dir;
    vec3 look_at;
    SPCameraMode mode;
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
        size_t instances;
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
    
    //WGPUBuffer uniform_staging_buffer;
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
} SPMaterialDesc;

typedef struct SPMaterialID {
    uint32_t id;
} SPMaterialID;

typedef struct SPTransform {
    vec3 pos; // 12 bytes
    vec3 rot; // 12 bytes
    vec3 scale; // 12 bytes
} SPTransform; // 36 bytes

typedef struct SPInstance {
    SPMeshID mesh;
    SPMaterialID material;
    SPTransform transform;
} SPInstance;

typedef struct SPInstanceDesc {
    SPMeshID mesh;
    SPMaterialID material;
    const SPTransform* transform;
} SPInstanceDesc;

typedef struct SPInstanceID {
    uint32_t id;
} SPInstanceID;

typedef struct _SPPools {
    _SPPool material_pool;
    SPMaterial* materials;
    _SPPool mesh_pool;
    SPMesh* meshes;
    _SPPool instance_pool;
    SPInstance* instances;
} _SPPools;

#define SP_STAGING_POOL_SIZE 16
#define SP_STAGING_POOL_MAPPINGS_UNTIL_NEXT_CHECK 255

typedef struct _SPStagingBufferPool {
    uint32_t num_bytes;
    uint8_t count;
    uint8_t cur;
    uint8_t max_cur;
    uint8_t mappings_until_next_check;
    WGPUBuffer buffer[SP_STAGING_POOL_SIZE];
    uint8_t* data[SP_STAGING_POOL_SIZE];
    bool waiting_for_map[SP_STAGING_POOL_SIZE];
} _SPStagingBufferPool;

typedef struct _SPBuffers {
    struct {
        WGPUBuffer common;
        WGPUBuffer dynamic;
        _SPStagingBufferPool common_staging;
        _SPStagingBufferPool dynamic_staging;
    } uniform;
} _SPBuffers;    

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
    _SPBuffers buffers;
    SPCamera active_cam;

    SPInstanceID active_instance;

    WGPUTextureView depth_view;

    struct {
        uint32_t width;
        uint32_t height;
    } surface_size;

    uint32_t dynamic_alignment;

    uint32_t frame_index;
} _sp_state;

#define DEBUG_PRINT(should_print, ...) do{if(should_print){ printf("[%u] ", _sp_state.frame_index);printf(__VA_ARGS__); }}while(0)

#define DEBUG_PRINT_MAT4(should_print, name, mat) \
do{ \
    if(should_print){\
        printf("[%u] %s:\n", name); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]); \
    } \
} while(0)

void errorCallback(WGPUErrorType type, char const * message, void * userdata) {
    printf("%d: %s\n", type, message);
}

// PUBLIC
/* 
Initializes the application and creates all static resources
*/
void spInit(const SPInitDesc* desc);
/* 
Updates the model, view and projection matrices and copies them
in a mapped staging buffer
*/
void spUpdate(void);
/*
Records the commands for the GPU and submits them
Includes copying from staging to GPU-only buffers
Draws all instances created with spCreateInstance
*/
void spRender(void);
/*
Creates a mesh and returns an identifier to it
*/
SPMeshID spCreateMesh(const SPMeshDesc* desc);
/*
Creates a material and returns an identifier to it
*/
SPMaterialID spCreateMaterial(const SPMaterialDesc* desc);
/*
Creates an instance and returns an identifier to it
*/
SPInstanceID spCreateInstance(const SPInstanceDesc* desc);

/*
Returns a temporary pointer to the active camera 
*/
SPCamera* spGetActiveCamera();
/*
Returns a temporary pointer to the instance with the specified id
NULL if not a valid id
*/
SPInstance* spGetInstance(SPInstanceID instance_id);

// PRIVATE
// Pools from https://github.com/floooh/sokol/
/* Setup all pools */
void _spSetupPools(_SPPools* pools, const SPPoolsDesc* pools_desc);
/* Inititalize a pool */
void _spInitPool(_SPPool* pool, size_t size);
/* Discard a pool */
void _spDiscardPool(_SPPool* pool);
/* Get the next free ID from a pool */
int _spAllocPoolIndex(_SPPool* pool);
/* Free an ID from a pool */
void _spFreePoolIndex(_SPPool* pool, int slot_index);

// Matrices
/* Updates the view matrix */
void _spUpdateView(void);
/* Updates the projection matrix */
void _spUpdateProjection(void);

// Staging buffer pools
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing dynamic model data */
void _spUpdateStagingPoolDynamicCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing common data */
void _spUpdateStagingPoolCommonCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
/* Releases all staging buffers */
void _spDiscardStagingBuffers();
/* Updates a pool
 - finding a mapped buffer
   - otherwise create a new mapped buffer
 - release unused buffer
 - pool->cur is now the index to a valid mapped buffer
 */
void _spUpdateStagingPool(_SPStagingBufferPool* pool, WGPUBufferMapWriteCallback callback);
/* Creates a model matrix for each instance 
and copies them to the current mapped 'dynamic' staging buffer */
void _spUpdateUBODynamic(void);
/* Copies the view and projection matrices to the current mapped 'common' staging buffer  */
void _spUpdateUBOCommon(void);

// General
/*
Calls all internal updates
*/
void _spUpdate(void);
/*
Creates a command buffer from the recorded commands
and submits it to the queue
Recreates the command encoder
*/
void _spSubmit(void);

// IMPLEMENTATION

// PUBLIC
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

    _sp_state.surface_size.width = desc->surface_size.width;
    _sp_state.surface_size.height = desc->surface_size.height;

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

    // Uniform buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = sizeof(UBOCommon),
        };

       _sp_state.buffers.uniform.common = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    
    _sp_state.buffers.uniform.common_staging.count = 0;
    _sp_state.buffers.uniform.common_staging.cur = 0;
    _sp_state.buffers.uniform.common_staging.num_bytes = sizeof(UBOCommon);

    _sp_state.dynamic_alignment = 256;// sizeof(UBODynamic);
    const uint32_t instance_count = (_sp_state.pools.instance_pool.size - 1);

    // Uniform buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = _sp_state.dynamic_alignment * instance_count,
        };

        _sp_state.buffers.uniform.dynamic = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    _sp_state.buffers.uniform.dynamic_staging.count = 0;
    _sp_state.buffers.uniform.dynamic_staging.cur = 0;
    _sp_state.buffers.uniform.dynamic_staging.num_bytes = _sp_state.dynamic_alignment * instance_count;

    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created uniform_buffer with size %u\n", _sp_state.dynamic_alignment * instance_count);

    _sp_state.active_cam = desc->camera;

    WGPUTextureDescriptor texture_desc = {
        .usage = WGPUTextureUsage_OutputAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = _sp_state.surface_size.width,
            .height = _sp_state.surface_size.height,
            .depth = 1,
        },
        .arrayLayerCount = 1,
        .format = WGPUTextureFormat_Depth32Float,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    WGPUTexture depth_texture = wgpuDeviceCreateTexture(_sp_state.device, &texture_desc);

    WGPUTextureViewDescriptor view_desc = {
        .format = WGPUTextureFormat_Depth32Float,
        .dimension = WGPUTextureViewDimension_2D,
        .aspect = WGPUTextureAspect_All,
    };
    _sp_state.depth_view = wgpuTextureCreateView(depth_texture, &view_desc);
    
}

void spUpdate(void) {
    _spUpdate();
}

void spRender(void) {
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: start\n");
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.common_staging.buffer[_sp_state.buffers.uniform.common_staging.cur], 0, 
        _sp_state.buffers.uniform.common, 0, 
        _sp_state.buffers.uniform.common_staging.num_bytes
    ); 
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy common ubo\n");
    
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.dynamic_staging.buffer[_sp_state.buffers.uniform.dynamic_staging.cur], 0, 
        _sp_state.buffers.uniform.dynamic, 0, 
        _sp_state.buffers.uniform.dynamic_staging.num_bytes
    );
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy dynamic ubo\n");

    WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);

    // main pass
    // ***
    WGPURenderPassColorAttachmentDescriptor color_attachment = {
        .attachment = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearColor = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f}
    };

    WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
        .attachment = _sp_state.depth_view,
        .depthLoadOp = WGPULoadOp_Clear,
        .depthStoreOp = WGPUStoreOp_Store,
        .clearDepth = 1.0f,
        .stencilLoadOp = WGPULoadOp_Clear,
        .stencilStoreOp = WGPUStoreOp_Store,
        .clearStencil = 0,
    };

    WGPURenderPassDescriptor render_pass = {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = &depth_attachment,
    };
    uint32_t index = 0;
    _sp_state.pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
    for(uint32_t i = 1; i < _sp_state.pools.instance_pool.size; i++) {
        SPMeshID mesh_id = _sp_state.pools.instances[i].mesh;
        SPMaterialID mat_id = _sp_state.pools.instances[i].material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        SPMesh* mesh = &(_sp_state.pools.meshes[mesh_id.id]);
        SPMaterial* material = &(_sp_state.pools.materials[mat_id.id]);

        uint32_t offsets[] = {index * _sp_state.dynamic_alignment};

        DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: obj %u with offset %u\n", i, offsets[0]);

        wgpuRenderPassEncoderSetPipeline(_sp_state.pass_enc, material->pipeline);
        wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 0, material->bind_group, ARRAY_LEN(offsets), offsets);

        wgpuRenderPassEncoderSetVertexBuffer(_sp_state.pass_enc, 0, mesh->vertex_buffer, 0);
        wgpuRenderPassEncoderSetIndexBuffer(_sp_state.pass_enc, mesh->index_buffer, 0);
        wgpuRenderPassEncoderDrawIndexed(_sp_state.pass_enc, mesh->indices_count, 1, 0, 0, 0);

        index++;
    }
    wgpuRenderPassEncoderEndPass(_sp_state.pass_enc);
    wgpuRenderPassEncoderRelease(_sp_state.pass_enc);
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
    _spSubmit();
    _sp_state.frame_index++;
    // ***
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
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created vert shader\n");
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
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created frag shader\n");

    WGPUBindGroupLayoutBinding vert_bglbs[] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = true,
            .multisampled = false,
            .textureDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Vertex,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = false,
            .multisampled = false,
            .textureDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        }
    };

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .bindingCount = ARRAY_LEN(vert_bglbs),
        .bindings = vert_bglbs
    };
    material->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &bgl_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created vert bgl\n");
    material->frag.bind_group_layout = NULL;

    WGPUBindGroupBinding bindings[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.dynamic,
            .offset = 0,
            .size = sizeof(UBODynamic),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = _sp_state.buffers.uniform.common,
            .offset = 0,
            .size = sizeof(UBOCommon),
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
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created bind group\n");

    WGPUBindGroupLayout blgs[] = {
        material->vert.bind_group_layout
    };

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = ARRAY_LEN(blgs),
        .bindGroupLayouts = blgs
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
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(Vertex, normal),
            .shaderLocation = 2
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

    WGPUDepthStencilStateDescriptor depth_stencil_state_desc = {
        .depthWriteEnabled = true,
        .format = WGPUTextureFormat_Depth32Float,
        .depthCompare = WGPUCompareFunction_Less,
        .stencilFront = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep,
        },
        .stencilBack = {
            .compare = WGPUCompareFunction_Always,
            .failOp = WGPUStencilOperation_Keep,
            .depthFailOp = WGPUStencilOperation_Keep,
            .passOp = WGPUStencilOperation_Keep,
        },
        .stencilReadMask = 0xFFFFFFFF,
        .stencilWriteMask = 0xFFFFFFFF,
    };

    WGPURasterizationStateDescriptor rast_state_desc = {
        .nextInChain = NULL,
        .frontFace = WGPUFrontFace_CW,
        .cullMode = WGPUCullMode_Front,
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
        .sampleCount = 1,
        .depthStencilState = &depth_stencil_state_desc,
        .colorStateCount = 1,
        .colorStates = &color_state_desc,
        .primitiveTopology = WGPUPrimitiveTopology_TriangleList,
        .sampleMask = 0xFFFFFFFF
    };
    material->pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created pipeline\n");
    return material_id;
}

SPInstanceID spCreateInstance(const SPInstanceDesc* desc) {
    SPIDER_ASSERT(desc->mesh.id != SP_INVALID_ID && desc->material.id != SP_INVALID_ID);
    if(!desc->mesh.id || !desc->material.id) {
        return (SPInstanceID){SP_INVALID_ID};
    }
    SPInstanceID instance_id = (SPInstanceID){_spAllocPoolIndex(&(_sp_state.pools.instance_pool))};
    if(instance_id.id == SP_INVALID_ID) {
        return instance_id;
    }
    int id = instance_id.id; 
    SPInstance* instance = &(_sp_state.pools.instances[id]);
    instance->mesh = desc->mesh;
    instance->material = desc->material;
    if(desc->transform) {
        memcpy(&(instance->transform), desc->transform, sizeof(SPTransform));
    }
    else {
        instance->transform = (SPTransform){
            .scale = {1.0f, 1.0f, 1.0f},
        };
    }
    return instance_id;
}

SPCamera* spGetActiveCamera() {
    return &(_sp_state.active_cam);
}

SPInstance* spGetInstance(SPInstanceID instance_id) {
    if(instance_id.id == SP_INVALID_ID || instance_id.id >= _sp_state.pools.instance_pool.size) {
        return NULL;
    }
    return &(_sp_state.pools.instances[instance_id.id]);
}

// PRIVATE
// Pool implementation from https://github.com/floooh/sokol/ 
// ***
void _spSetupPools(_SPPools* pools, const SPPoolsDesc* pools_desc) {
    _spInitPool(&(_sp_state.pools.mesh_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.meshes, _SP_MESH_POOL_MAX));
    size_t mesh_pool_byte_size = sizeof(SPMesh) * pools->mesh_pool.size;
    pools->meshes = (SPMesh*) SPIDER_MALLOC(mesh_pool_byte_size);
    SPIDER_ASSERT(pools->meshes);
    memset(pools->meshes, 0, mesh_pool_byte_size);

    
    _spInitPool(&(_sp_state.pools.material_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.materials, _SP_MATERIAL_POOL_MAX));
    size_t mat_pool_byte_size = sizeof(SPMaterial) * pools->material_pool.size;
    pools->materials = (SPMaterial*) SPIDER_MALLOC(mat_pool_byte_size);
    SPIDER_ASSERT(pools->materials);
    memset(pools->materials, 0, mat_pool_byte_size);

    _spInitPool(&(_sp_state.pools.instance_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.instances, _SP_RENDER_OBJECT_POOL_MAX));
    size_t instance_pool_byte_size = sizeof(SPInstance) * pools->instance_pool.size;
    pools->instances = (SPInstance*) SPIDER_MALLOC(instance_pool_byte_size);
    SPIDER_ASSERT(pools->instances);
    memset(pools->instances, 0, instance_pool_byte_size);
}

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
// ***

void _spUpdateView(void) {
    vec3 up = { 0.0f, 1.0f, 0.0f };
    if(_sp_state.active_cam.mode == SPCameraMode_Direction) {
        glm_look(
            _sp_state.active_cam.pos,
            _sp_state.active_cam.dir,
            up,
            _sp_state.active_cam._view
        );
    } 
    else {
        glm_lookat(
            _sp_state.active_cam.pos,
            _sp_state.active_cam.look_at,
            up,
            _sp_state.active_cam._view
        );
    }
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

#define _SP_CREATE_STAGING_POOL_CALLBACK_IMPL(Pool, PoolName) \
void _spUpdateStagingPool##PoolName##Callback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata) { \
    int index = (int)userdata; \
    if(index < Pool.count) { \
        Pool.data[index] = (uint8_t*)data; \
    } \
 }

_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.dynamic_staging, Dynamic)
_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.common_staging, Common)

void _spDiscardStagingBuffers() {
    for(uint8_t i = 0; i < _sp_state.buffers.uniform.common_staging.count; i++) {
        wgpuBufferRelease(_sp_state.buffers.uniform.common_staging.buffer[i]);
        _sp_state.buffers.uniform.common_staging.buffer[i] = NULL;
        _sp_state.buffers.uniform.common_staging.data[i] = NULL;
    }
    _sp_state.buffers.uniform.common_staging.count = 0;
    for(uint8_t i = 0; i < _sp_state.buffers.uniform.dynamic_staging.count; i++) {
        wgpuBufferRelease(_sp_state.buffers.uniform.dynamic_staging.buffer[i]);
        _sp_state.buffers.uniform.dynamic_staging.buffer[i] = NULL;
        _sp_state.buffers.uniform.dynamic_staging.data[i] = NULL;
    }
    _sp_state.buffers.uniform.dynamic_staging.count = 0;
}

void _spUpdateStagingPool(_SPStagingBufferPool* pool, WGPUBufferMapWriteCallback callback) {
    if(pool->count) {
        wgpuBufferMapWriteAsync(
            pool->buffer[pool->cur], 
            callback, 
            (void*)(int)pool->cur
        );
    }

    bool found = false;
    for(uint8_t i = 0; i < pool->count; i++) {
        if(pool->data[i]) {
            pool->cur = i;
            if(pool->cur > pool->max_cur) {
                pool->max_cur = pool->cur;
                pool->mappings_until_next_check = SP_STAGING_POOL_MAPPINGS_UNTIL_NEXT_CHECK;
            }
            found = true;
            break;
        }
    }

    if(!found) {
        pool->mappings_until_next_check = SP_STAGING_POOL_MAPPINGS_UNTIL_NEXT_CHECK;
        SPIDER_ASSERT(pool->count < SP_STAGING_POOL_SIZE);
        pool->cur = pool->count++;
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc,
            .size = pool->num_bytes,
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
        pool->buffer[pool->cur] = result.buffer;
        pool->data[pool->cur] = (uint8_t*) result.data;
       
        DEBUG_PRINT(DEBUG_PRINT_GENERAL, "staging buffers: created new buffer (%u in pool) with size %u\n", pool->cur, pool->num_bytes);
    }

    if(--pool->mappings_until_next_check == 0) {
        pool->mappings_until_next_check = SP_STAGING_POOL_MAPPINGS_UNTIL_NEXT_CHECK;
        if(pool->max_cur < pool->count) {
            pool->count--;
            wgpuBufferRelease(pool->buffer[pool->count]);
            pool->buffer[pool->count] = NULL;
            pool->data[pool->count] = NULL;

            DEBUG_PRINT(DEBUG_PRINT_GENERAL, "staging buffers: released unused buffer (%u in pool) with size %u\n", pool->count, pool->num_bytes);
        }
    }
}

void _spUpdateUBODynamic(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.dynamic_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolDynamicCallback);
    uint32_t index = 0;
    for(size_t i = 1; i < _sp_state.pools.instance_pool.size; i++) {
        SPMeshID mesh_id = _sp_state.pools.instances[i].mesh;
        SPMaterialID mat_id = _sp_state.pools.instances[i].material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        SPInstance* instance = &(_sp_state.pools.instances[i]);
        UBODynamic ubo = {
            .model = GLM_MAT4_IDENTITY_INIT,
        };
        mat4 scale = GLM_MAT4_IDENTITY_INIT;
        glm_scale(scale, instance->transform.scale);
        vec3 rot_rad = {
            glm_rad(instance->transform.rot[0]),
            glm_rad(instance->transform.rot[1]),
            glm_rad(instance->transform.rot[2])
        };
        mat4 rot = GLM_MAT4_IDENTITY_INIT;
        glm_euler_zxy(rot_rad, rot);
        glm_mat4_mul(rot, scale, rot);
        glm_translate(ubo.model, instance->transform.pos);
        glm_mat4_mul(ubo.model, rot, ubo.model);
        uint64_t offset = index * _sp_state.dynamic_alignment;

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &ubo, sizeof(UBODynamic));
        
        index++;
    }
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdateUBOCommon(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.common_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolCommonCallback);

    UBOCommon ubo = {
        .view = GLM_MAT4_IDENTITY_INIT,
        .proj = GLM_MAT4_IDENTITY_INIT,
    };
    memcpy(ubo.view, _sp_state.active_cam._view, sizeof(mat4));
    memcpy(ubo.proj, _sp_state.active_cam._proj, sizeof(mat4));

    memcpy(pool->data[pool->cur], &ubo, sizeof(UBOCommon));

    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdate(void) {
    _spUpdateView();
    _spUpdateProjection();
    _spUpdateUBOCommon();
    _spUpdateUBODynamic();
}


void _spSubmit(void) {
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(_sp_state.cmd_enc, NULL);
    wgpuCommandEncoderRelease(_sp_state.cmd_enc);
    _sp_state.cmd_enc = NULL;

    wgpuQueueSubmit(_sp_state.queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    _sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
} 

#endif // SPIDER_H_