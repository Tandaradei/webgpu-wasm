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

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "helper.h"

#define SPIDER_ASSERT(assertion) assert(assertion)
#define SPIDER_MALLOC(size) malloc(size)

#define DEBUG_PRINT_ALLOWED 0
#define DEBUG_PRINT_TYPE_INIT 1
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 1
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 1
#define DEBUG_PRINT_RENDER 0
#define DEBUG_PRINT_METRICS 1

#define _SP_RELEASE_RESOURCE(Type, Name) if(Name) {wgpu##Type##Release(Name); Name = NULL;}

#include <cglm/cglm.h>

typedef struct SPVertex {
    vec3 pos;
    vec3 color;
    vec3 normal;
    vec2 tex_coords;
} SPVertex;

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
    size_t last_index_plus_1;
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

typedef struct _SPShaderStage {
    WGPUShaderModule module;
    WGPUBindGroupLayout bind_group_layout;
} _SPShaderStage;

typedef struct _SPRenderPipeline {
    _SPShaderStage vert;
    _SPShaderStage frag;
    WGPUBindGroup bind_group;
    WGPURenderPipeline pipeline;
} _SPRenderPipeline;

typedef struct SPMaterialProperties {
    float specular;
} SPMaterialProperties;

typedef struct _SPMaterialTexture {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUSampler sampler;
} _SPMaterialTexture;

typedef struct SPMaterial {
    struct {
        WGPUBindGroup vert;
        WGPUBindGroup frag;
    } bind_groups;
    SPMaterialProperties props;
    _SPMaterialTexture diffuse;
} SPMaterial;

typedef struct SPTextureFileDesc {
    const char* name;
    uint32_t width;
    uint32_t height;
    uint8_t channel_count;
} SPTextureFileDesc;

typedef struct SPMaterialDesc {
    const float specular;
    SPTextureFileDesc diffuse_tex;
} SPMaterialDesc;

typedef struct SPMaterialID {
    uint32_t id;
} SPMaterialID;

typedef struct _SPUboCommon {
    mat4 view;
    mat4 proj;
} _SPUboCommon;


typedef struct _SPUboDynamic {
    mat4 model;
} _SPUboDynamic;

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

#define SP_STAGING_POOL_SIZE 64
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
        _SPStagingBufferPool common_staging;
        WGPUBuffer dynamic;
        _SPStagingBufferPool dynamic_staging;
        WGPUBuffer material;
        _SPStagingBufferPool material_staging;
    } uniform;
} _SPBuffers;    

#define _SP_GET_DEFAULT_IF_ZERO(value, default_value) value ? value : default_value 

#define _SP_MATERIAL_POOL_MAX 8
#define _SP_MESH_POOL_MAX 256
#define _SP_RENDER_OBJECT_POOL_MAX 256

typedef struct _SPState {
    WGPUDevice device;
    WGPUQueue queue;

    WGPUInstance instance;
    WGPUSurface surface;
    
    WGPUSwapChain swap_chain;
    WGPUCommandEncoder cmd_enc;
    WGPURenderPassEncoder pass_enc;

    WGPUTextureView depth_view;

    _SPPools pools;
    _SPBuffers buffers;
    SPCamera active_cam;

    struct {
        uint32_t width;
        uint32_t height;
    } surface_size;

    uint32_t dynamic_alignment;

    uint32_t frame_index;

    struct {
        struct {
            _SPRenderPipeline forward;
            _SPRenderPipeline shadow;
        } render;
    } pipelines;

    uint32_t* instance_counts_per_mat;
    SPInstanceID* sorted_instances;
    
    // TODO move to _SPRenderPipeline (?)
    WGPUBindGroup shadow_dir_bind_group;
    WGPUBuffer ubo_common_light;
    WGPUTexture shadow_dir_texture;
    WGPUTextureView shadow_dir_view;
    WGPUTextureView shadow_dir_color_view;
} _SPState;
_SPState _sp_state;

#define DEBUG_PRINT(should_print, ...) do{if(DEBUG_PRINT_ALLOWED && should_print){ printf("[%u] ", _sp_state.frame_index);printf(__VA_ARGS__); }}while(0)

#define DEBUG_PRINT_MAT4(should_print, name, mat) \
do{ \
    if(DEBUG_PRINT_ALLOWED && should_print){\
        printf("[%u] %s:\n", _sp_state.frame_index, name); \
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
Releases all remaining resources
*/
void spShutdown(void);
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

//
/*
Loads an image with tex_file_desc info
Creates a WGPUTexture object and stores it in mat_tex.texture
Creates an temporary buffer and stores it in out_buffer
Fills the buffer with the image data
Creates a command on cmd_enc to copy from buffer to texture
*/
void _spCreateAndLoadTexture(_SPMaterialTexture* mat_tex, SPTextureFileDesc tex_file_desc, WGPUBuffer* out_buffer, WGPUCommandEncoder cmd_enc);

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
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing material data */
void _spUpdateStagingPoolMaterialCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
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
void _spUpdate_SPUBOCommon(void);
/* Copies the material properties for each material to the current mapped 'material' staging buffer  */
void _spUpdateUBOMaterial(void);

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

void _spSortInstances(void);

void _spDrawAllInstances(WGPURenderPassEncoder render_pass);

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
            .size = sizeof(_SPUboCommon),
        };

       _sp_state.buffers.uniform.common = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    
    _sp_state.buffers.uniform.common_staging.count = 0;
    _sp_state.buffers.uniform.common_staging.cur = 0;
    _sp_state.buffers.uniform.common_staging.num_bytes = sizeof(_SPUboCommon);

    _sp_state.dynamic_alignment = 256;// sizeof(_SPUboDynamic);
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

    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created uniform_buffer with size %u\n", _sp_state.buffers.uniform.dynamic_staging.num_bytes);

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

    // Shadow map pipeline
    // ***
    {
        _SPRenderPipeline* pipeline = &(_sp_state.pipelines.render.shadow);
        {
            FileReadResult vertShader;
            readFile("src/shaders/compiled/shadow_dir.vert.spv", &vertShader);
            DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", vertShader.size);
            WGPUShaderModuleDescriptor sm_desc = {
                .codeSize = vertShader.size / sizeof(uint32_t),
                .code = (const uint32_t*)vertShader.data
            };
            pipeline->vert.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
        }
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow dir: created vert shader\n");

        {
            FileReadResult fragShader;
            readFile("src/shaders/compiled/shadow_dir.frag.spv", &fragShader);
            DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", fragShader.size);
            WGPUShaderModuleDescriptor sm_desc = {
                .codeSize = fragShader.size / sizeof(uint32_t),
                .code = (const uint32_t*)fragShader.data
            };
            pipeline->frag.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
        }
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow dir: created frag shader\n");

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
            },
        };

        WGPUBindGroupLayoutDescriptor vert_bgl_desc = {
            .bindingCount = ARRAY_LEN(vert_bglbs),
            .bindings = vert_bglbs
        };
        pipeline->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &vert_bgl_desc);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert bind group layout\n");

        WGPUBindGroupLayout bgls[] = {
            pipeline->vert.bind_group_layout
        };

        WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
            .bindGroupLayoutCount = ARRAY_LEN(bgls),
            .bindGroupLayouts = bgls
        };
        
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform,
            .size = sizeof(_SPUboCommon),
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
        _sp_state.ubo_common_light = result.buffer;
        _SPUboCommon light_ubo = {
            .view = GLM_MAT4_IDENTITY_INIT,
            .proj = GLM_MAT4_IDENTITY_INIT
        };
        glm_lookat(
            (vec3){-10.0f, 10.0f, 0.0f},
            (vec3){0.0f, 0.0f, 0.0f},
            (vec3){0.0f, 1.0f, 0.0f},
            light_ubo.view
        );
        glm_perspective(
            glm_rad(60.0f),
            1.0f,
            0.1f,
            100.0f,
            light_ubo.proj
        );
        memcpy(result.data, &light_ubo, result.dataLength);
        wgpuBufferUnmap(result.buffer);

        WGPUBindGroupBinding vert_bindings[] = {
            {
                .binding = 0,
                .buffer = _sp_state.buffers.uniform.dynamic,
                .offset = 0,
                .size = sizeof(_SPUboDynamic),
                .sampler = NULL,
                .textureView = NULL,
            },
            {
                .binding = 1,
                .buffer = _sp_state.ubo_common_light,
                .offset = 0,
                .size = sizeof(_SPUboCommon),
                .sampler = NULL,
                .textureView = NULL,
            },
        };

        WGPUBindGroupDescriptor vert_bg_desc = {
            .layout = pipeline->vert.bind_group_layout,
            .bindingCount = ARRAY_LEN(vert_bindings),
            .bindings = vert_bindings
        };
        _sp_state.shadow_dir_bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);

        WGPUVertexAttributeDescriptor vertex_attribute_descs[] = {
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(SPVertex, pos),
                .shaderLocation = 0
            },
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(SPVertex, color),
                .shaderLocation = 1
            },
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(SPVertex, normal),
                .shaderLocation = 2
            },
            {
                .format = WGPUVertexFormat_Float2,
                .offset = offsetof(SPVertex, tex_coords),
                .shaderLocation = 3
            }
        };

        WGPUVertexBufferLayoutDescriptor vert_buffer_layout_descs[] = {
            {
                .arrayStride = sizeof(SPVertex),
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

        WGPUProgrammableStageDescriptor frag_stage_desc = {
            .module = pipeline->frag.module,
            .entryPoint = "main"
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
            .depthBias = 1,
            .depthBiasSlopeScale = 1.75f,
            .depthBiasClamp = 0.0f,
        };

        WGPUColorStateDescriptor color_state_desc = {
            .format = WGPUTextureFormat_R32Float,
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

        WGPURenderPipelineDescriptor pipeline_desc = {
            .layout = wgpuDeviceCreatePipelineLayout(_sp_state.device, &pipeline_layout_desc),
            .vertexStage.module = pipeline->vert.module,
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
        pipeline->pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created shadow dir pipeline\n");

        WGPUExtent3D texture_size = {
            .width = 1024,
            .height = 1024,
            .depth = 1,
        };

        WGPUTextureDescriptor tex_desc = {
            .usage = WGPUTextureUsage_Sampled | WGPUTextureUsage_OutputAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = texture_size,
            .arrayLayerCount = 1,
            .format = WGPUTextureFormat_Depth32Float,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

        _sp_state.shadow_dir_texture = wgpuDeviceCreateTexture(_sp_state.device, &tex_desc);

        WGPUTextureViewDescriptor tex_view_desc = {
            .format = WGPUTextureFormat_Depth32Float,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 0,
            .baseArrayLayer = 0,
            .arrayLayerCount = 0,
            .aspect = WGPUTextureAspect_All,
        };

        _sp_state.shadow_dir_view = wgpuTextureCreateView(_sp_state.shadow_dir_texture, &tex_view_desc);

        WGPUTextureDescriptor tex_color_desc = {
            .usage = WGPUTextureUsage_Sampled | WGPUTextureUsage_OutputAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = texture_size,
            .arrayLayerCount = 1,
            .format = WGPUTextureFormat_R32Float,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

        WGPUTexture color_tex = wgpuDeviceCreateTexture(_sp_state.device, &tex_color_desc);

        WGPUTextureViewDescriptor tex_color_view_desc = {
            .format = WGPUTextureFormat_R32Float,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 0,
            .baseArrayLayer = 0,
            .arrayLayerCount = 0,
            .aspect = WGPUTextureAspect_All,
        };

        _sp_state.shadow_dir_color_view = wgpuTextureCreateView(color_tex, &tex_color_view_desc);
    }
    // ***

    // Default render pipeline
    // ***
    {
        // Uniform buffer creation
        uint32_t material_count = _sp_state.pools.material_pool.size - 1;
        {
            WGPUBufferDescriptor buffer_desc = {
                .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
                .size = _sp_state.dynamic_alignment * material_count,
            };

            _sp_state.buffers.uniform.material = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
        }
        _sp_state.buffers.uniform.material_staging.count = 0;
        _sp_state.buffers.uniform.material_staging.cur = 0;
        _sp_state.buffers.uniform.material_staging.num_bytes = _sp_state.dynamic_alignment * material_count;

        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created uniform_buffer with size %u\n", _sp_state.buffers.uniform.material_staging.num_bytes);
        
        _SPRenderPipeline* pipeline = &(_sp_state.pipelines.render.forward);
        {
            FileReadResult vertShader;
            readFile("src/shaders/compiled/forward.vert.spv", &vertShader);
            DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", vertShader.size);
            WGPUShaderModuleDescriptor sm_desc = {
                .codeSize = vertShader.size / sizeof(uint32_t),
                .code = (const uint32_t*)vertShader.data
            };
            pipeline->vert.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
        }
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert shader\n");
        {
            FileReadResult fragShader;
            readFile("src/shaders/compiled/forward.frag.spv", &fragShader);
            DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", fragShader.size);
            WGPUShaderModuleDescriptor sm_desc = {
                .codeSize = fragShader.size / sizeof(uint32_t),
                .code = (const uint32_t*)fragShader.data
            };
            pipeline->frag.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_desc);
        }
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created frag shader\n");

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
            },
            {
                .binding = 2,
                .visibility = WGPUShaderStage_Vertex,
                .type = WGPUBindingType_UniformBuffer,
                .hasDynamicOffset = false,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_Undefined,
                .textureComponentType = WGPUTextureComponentType_Float,
            },
        };

        WGPUBindGroupLayoutDescriptor vert_bgl_desc = {
            .bindingCount = ARRAY_LEN(vert_bglbs),
            .bindings = vert_bglbs
        };
        pipeline->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &vert_bgl_desc);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert bind group layout\n");
        
        WGPUBindGroupLayoutBinding frag_bglbs[] = {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Fragment,
                .type = WGPUBindingType_UniformBuffer,
                .hasDynamicOffset = true,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_Undefined,
                .textureComponentType = WGPUTextureComponentType_Float,
            },
            {
                .binding = 1,
                .visibility = WGPUShaderStage_Fragment,
                .type = WGPUBindingType_Sampler,
                .hasDynamicOffset = false,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_2D,
                .textureComponentType = WGPUTextureComponentType_Float,
            },
            {
                .binding = 2,
                .visibility = WGPUShaderStage_Fragment,
                .type = WGPUBindingType_SampledTexture,
                .hasDynamicOffset = false,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_2D,
                .textureComponentType = WGPUTextureComponentType_Float,
            },
            {
                .binding = 3,
                .visibility = WGPUShaderStage_Fragment,
                .type = WGPUBindingType_SampledTexture,
                .hasDynamicOffset = false,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_2D,
                .textureComponentType = WGPUTextureComponentType_Float,
            },
        };

        WGPUBindGroupLayoutDescriptor frag_bgl_desc = {
            .bindingCount = ARRAY_LEN(frag_bglbs),
            .bindings = frag_bglbs
        };
        pipeline->frag.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &frag_bgl_desc);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created frag bind group layout\n");

        WGPUBindGroupLayout bgls[] = {
            pipeline->vert.bind_group_layout,
            pipeline->frag.bind_group_layout
        };

        WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
            .bindGroupLayoutCount = ARRAY_LEN(bgls),
            .bindGroupLayouts = bgls
        };

        WGPUProgrammableStageDescriptor frag_stage_desc = {
            .module = pipeline->frag.module,
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
                .offset = offsetof(SPVertex, pos),
                .shaderLocation = 0
            },
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(SPVertex, color),
                .shaderLocation = 1
            },
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(SPVertex, normal),
                .shaderLocation = 2
            },
            {
                .format = WGPUVertexFormat_Float2,
                .offset = offsetof(SPVertex, tex_coords),
                .shaderLocation = 3
            }
        };

        WGPUVertexBufferLayoutDescriptor vert_buffer_layout_descs[] = {
            {
                .arrayStride = sizeof(SPVertex),
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
            .vertexStage.module = pipeline->vert.module,
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
        pipeline->pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created forward render pipeline\n");
        _sp_state.instance_counts_per_mat = SPIDER_MALLOC(sizeof(uint32_t) * desc->pools.capacities.materials);
        _sp_state.sorted_instances = SPIDER_MALLOC((sizeof(SPInstanceID) * desc->pools.capacities.materials) * desc->pools.capacities.instances);
    }
    // ***
}

void spShutdown(void) {
    for(uint32_t i = 0; i < _sp_state.pools.mesh_pool.size; i++) {
        SPMesh* mesh = &(_sp_state.pools.meshes[i]);
        if(!mesh) {
            continue;
        }
        _SP_RELEASE_RESOURCE(Buffer, mesh->vertex_buffer)
        _SP_RELEASE_RESOURCE(Buffer, mesh->index_buffer)
    }
    _spDiscardPool(&_sp_state.pools.mesh_pool);
    free(_sp_state.pools.meshes);
    _sp_state.pools.meshes = NULL;

    for(uint32_t i = 0; i < _sp_state.pools.material_pool.size; i++) {
        SPMaterial* material = &(_sp_state.pools.materials[i]);
        if(!material) {
            continue;
        }
        _SP_RELEASE_RESOURCE(BindGroup, material->bind_groups.vert)
        _SP_RELEASE_RESOURCE(BindGroup, material->bind_groups.frag)
        _SP_RELEASE_RESOURCE(Sampler, material->diffuse.sampler)
        _SP_RELEASE_RESOURCE(Texture, material->diffuse.texture)
        _SP_RELEASE_RESOURCE(TextureView, material->diffuse.view)
    }
    _spDiscardPool(&_sp_state.pools.material_pool);
    free(_sp_state.pools.materials);
    _sp_state.pools.materials = NULL;

    // Instances don't have resources that need to be released/freed
    _spDiscardPool(&_sp_state.pools.instance_pool);
    free(_sp_state.pools.instances);
    _sp_state.pools.instances = NULL;

    free(_sp_state.sorted_instances);
    _sp_state.sorted_instances = NULL;
    free(_sp_state.instance_counts_per_mat);
    _sp_state.instance_counts_per_mat = NULL;

    _spDiscardStagingBuffers();
    wgpuBufferRelease(_sp_state.buffers.uniform.common);
    wgpuBufferRelease(_sp_state.buffers.uniform.dynamic);
    wgpuBufferRelease(_sp_state.buffers.uniform.material);

    _SP_RELEASE_RESOURCE(Device, _sp_state.device)
    _SP_RELEASE_RESOURCE(Queue, _sp_state.queue)
    _SP_RELEASE_RESOURCE(Instance, _sp_state.instance)
    _SP_RELEASE_RESOURCE(Surface, _sp_state.surface)
    _SP_RELEASE_RESOURCE(SwapChain, _sp_state.swap_chain)
    _SP_RELEASE_RESOURCE(CommandEncoder, _sp_state.cmd_enc)

    _SP_RELEASE_RESOURCE(RenderPipeline, _sp_state.pipelines.render.forward.pipeline)
    _SP_RELEASE_RESOURCE(BindGroup, _sp_state.pipelines.render.forward.bind_group)
    _SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.forward.vert.module)
    _SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.forward.vert.bind_group_layout)
    _SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.forward.frag.module)
    _SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.forward.frag.bind_group_layout)

    _SP_RELEASE_RESOURCE(RenderPipeline, _sp_state.pipelines.render.shadow.pipeline)
    _SP_RELEASE_RESOURCE(BindGroup, _sp_state.pipelines.render.shadow.bind_group)
    _SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.shadow.vert.module)
    _SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.shadow.vert.bind_group_layout)
    _SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.shadow.frag.module)
    _SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.shadow.frag.bind_group_layout)

    _SP_RELEASE_RESOURCE(BindGroup, _sp_state.shadow_dir_bind_group)
    _SP_RELEASE_RESOURCE(Buffer, _sp_state.ubo_common_light)
    _SP_RELEASE_RESOURCE(Texture, _sp_state.shadow_dir_texture)
    _SP_RELEASE_RESOURCE(TextureView, _sp_state.shadow_dir_view)
    _SP_RELEASE_RESOURCE(TextureView, _sp_state.shadow_dir_color_view)

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

    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.material_staging.buffer[_sp_state.buffers.uniform.material_staging.cur], 0, 
        _sp_state.buffers.uniform.material, 0, 
        _sp_state.buffers.uniform.material_staging.num_bytes
    );
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy material ubo\n");

    WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);
    
    _spSortInstances();

    // shadow dir
    // ***
    {
        WGPURenderPassColorAttachmentDescriptor color_attachment = {
            .attachment = _sp_state.shadow_dir_color_view,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearColor = (WGPUColor){1.0f, 1.0f, 1.0f, 1.0f}
        };

        WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
            .attachment = _sp_state.shadow_dir_view,
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
        WGPURenderPassEncoder shadow_pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
        wgpuRenderPassEncoderSetPipeline(shadow_pass_enc, _sp_state.pipelines.render.shadow.pipeline);
        _spDrawAllInstances(shadow_pass_enc);
        wgpuRenderPassEncoderEndPass(shadow_pass_enc);
        wgpuRenderPassEncoderRelease(shadow_pass_enc);
        DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
        _spSubmit();
    }
    // ***

    // main pass
    // ***
    #define MAIN_PASS 1
    #if MAIN_PASS
    {
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
        _sp_state.pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
        wgpuRenderPassEncoderSetPipeline(_sp_state.pass_enc, _sp_state.pipelines.render.forward.pipeline);

        uint32_t instances_count = _sp_state.pools.instance_pool.size - 1;
        for(uint32_t mat_id = 1; mat_id < _sp_state.pools.material_pool.size; mat_id++) {
            if(_sp_state.instance_counts_per_mat[mat_id - 1] == 0) {
                continue;
            }
            SPMaterial* material = &(_sp_state.pools.materials[mat_id]);
            uint32_t offsets_frag[] = { (mat_id - 1) * _sp_state.dynamic_alignment};
            wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 1, material->bind_groups.frag, ARRAY_LEN(offsets_frag), offsets_frag);
            SPMeshID last_mesh_id = {0};
            uint32_t instance_count = _sp_state.instance_counts_per_mat[mat_id - 1];
            for(uint32_t i = 0; i < instance_count; i++) {
                SPInstanceID ins_id = _sp_state.sorted_instances[(mat_id - 1) * (_sp_state.pools.instance_pool.size - 1) + i];
                SPInstance* instance = &(_sp_state.pools.instances[ins_id.id]);
                SPMeshID mesh_id = instance->mesh;
                SPMesh* mesh = &(_sp_state.pools.meshes[mesh_id.id]);
                if(last_mesh_id.id != mesh_id.id) {
                    wgpuRenderPassEncoderSetVertexBuffer(_sp_state.pass_enc, 0, mesh->vertex_buffer, 0);
                    wgpuRenderPassEncoderSetIndexBuffer(_sp_state.pass_enc, mesh->index_buffer, 0);
                    last_mesh_id = mesh_id;
                }
                uint32_t offsets_vert[] = { (ins_id.id - 1) * _sp_state.dynamic_alignment};
                wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 0, material->bind_groups.vert, ARRAY_LEN(offsets_vert), offsets_vert);
                wgpuRenderPassEncoderDrawIndexed(_sp_state.pass_enc, mesh->indices_count, 1, 0, 0, 0);
            }
        }

        wgpuRenderPassEncoderEndPass(_sp_state.pass_enc);
        wgpuRenderPassEncoderRelease(_sp_state.pass_enc);
        DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
        _spSubmit();
        wgpuTextureViewRelease(view);
    }
    #endif
    // ***
    
    _sp_state.frame_index++;
}

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

SPMaterialID spCreateMaterial(const SPMaterialDesc* desc) {
    SPMaterialID material_id = (SPMaterialID){_spAllocPoolIndex(&(_sp_state.pools.material_pool))};
    if(material_id.id == SP_INVALID_ID) {
        return material_id;
    }
    int id = material_id.id; 
    SPMaterial* material = &(_sp_state.pools.materials[id]);

    material->props.specular = desc->specular;
    WGPUBuffer temp_buffers[] = {
        NULL, // diffuse
    };
    WGPUCommandEncoder texture_load_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);

    _spCreateAndLoadTexture(&(material->diffuse), desc->diffuse_tex, &temp_buffers[0], texture_load_enc);

    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(texture_load_enc, NULL);
    wgpuCommandEncoderRelease(texture_load_enc);
    wgpuQueueSubmit(_sp_state.queue, 1, &cmd_buffer);
    wgpuCommandBufferRelease(cmd_buffer);
    for(uint8_t i = 0; i < ARRAY_LEN(temp_buffers); i++) {
        wgpuBufferRelease(temp_buffers[i]);
    }
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: submitted texture copies\n");

    WGPUTextureViewDescriptor tex_view_desc = {
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 0,
        .baseArrayLayer = 0,
        .arrayLayerCount = 0,
        .aspect = WGPUTextureAspect_All,
    };

    material->diffuse.view = wgpuTextureCreateView(material->diffuse.texture, &tex_view_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created texture view\n");

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = WGPUCompareFunction_LessEqual,
    };

    material->diffuse.sampler = wgpuDeviceCreateSampler(_sp_state.device, &sampler_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created sampler\n");
    
    WGPUBindGroupBinding vert_bindings[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.dynamic,
            .offset = 0,
            .size = sizeof(_SPUboDynamic),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = _sp_state.buffers.uniform.common,
            .offset = 0,
            .size = sizeof(_SPUboCommon),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 2,
            .buffer = _sp_state.ubo_common_light,
            .offset = 0,
            .size = sizeof(_SPUboCommon),
            .sampler = NULL,
            .textureView = NULL,
        },
    };

    WGPUBindGroupDescriptor vert_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.vert.bind_group_layout,
        .bindingCount = ARRAY_LEN(vert_bindings),
        .bindings = vert_bindings
    };
    material->bind_groups.vert = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created vert bind group\n");
    
    WGPUBindGroupBinding frag_bindings[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.material,
            .offset = 0,
            .size = sizeof(SPMaterialProperties),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = material->diffuse.sampler,
            .textureView = NULL,
        },
        {
            .binding = 2,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = material->diffuse.view,
        },
        {
            .binding = 3,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = _sp_state.shadow_dir_color_view,
        },
    };
    WGPUBindGroupDescriptor frag_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.frag.bind_group_layout,
        .bindingCount = ARRAY_LEN(frag_bindings),
        .bindings = frag_bindings
    };
    material->bind_groups.frag = wgpuDeviceCreateBindGroup(_sp_state.device, &frag_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created frag bind group\n");

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
    pool->last_index_plus_1 = 1;
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
        if(slot_index + 1 > pool->last_index_plus_1) {
            pool->last_index_plus_1 = slot_index + 1;
        }
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
    if(slot_index + 1 == pool->last_index_plus_1) {
        pool->last_index_plus_1--;
    }
}
// ***


void _spCreateAndLoadTexture(_SPMaterialTexture* mat_tex, SPTextureFileDesc tex_file_desc, WGPUBuffer* out_buffer, WGPUCommandEncoder cmd_enc) {
    int width = (int)tex_file_desc.width;
    int height = (int)tex_file_desc.height;
    int channel_count = (int)tex_file_desc.channel_count;
    stbi_uc* pixel_data = stbi_load(
        tex_file_desc.name,
        &width,
        &height,
        &channel_count,
        STBI_rgb_alpha
    );
    SPIDER_ASSERT(pixel_data);

    WGPUExtent3D texture_size = {
        .width = tex_file_desc.width,
        .height = tex_file_desc.height,
        .depth = 1,
    };

    WGPUTextureDescriptor tex_desc = {
        .usage = WGPUTextureUsage_Sampled | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = texture_size,
        .arrayLayerCount = 1,
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };

    mat_tex->texture = wgpuDeviceCreateTexture(_sp_state.device, &tex_desc);

    WGPUBufferDescriptor buffer_desc = {
        .usage = WGPUBufferUsage_CopySrc,
        .size = tex_file_desc.width * tex_file_desc.height * 4
    };

    WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
    memcpy(result.data, pixel_data, result.dataLength);
    stbi_image_free(pixel_data);
    wgpuBufferUnmap(result.buffer);
    *out_buffer = result.buffer;

    WGPUBufferCopyView buffer_copy_view = {
        .buffer = result.buffer,
        .offset = 0,
        .rowPitch = tex_file_desc.width * 4,
        .imageHeight = tex_file_desc.height,
    };

    WGPUTextureCopyView texture_copy_view = {
        .texture = mat_tex->texture,
        .mipLevel = 0,
        .arrayLayer = 0,
        .origin = {0, 0, 0},
    };
    wgpuCommandEncoderCopyBufferToTexture(cmd_enc, &buffer_copy_view, &texture_copy_view, &texture_size);
}


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
_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.material_staging, Material)

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
    for(uint8_t i = 0; i < _sp_state.buffers.uniform.material_staging.count; i++) {
        wgpuBufferRelease(_sp_state.buffers.uniform.material_staging.buffer[i]);
        _sp_state.buffers.uniform.material_staging.buffer[i] = NULL;
        _sp_state.buffers.uniform.material_staging.data[i] = NULL;
    }
    _sp_state.buffers.uniform.material_staging.count = 0;
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
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->data[pool->cur]);
    if(--pool->mappings_until_next_check == 0) {
        pool->mappings_until_next_check = SP_STAGING_POOL_MAPPINGS_UNTIL_NEXT_CHECK;
        if(pool->max_cur < pool->count - 1) {
            pool->max_cur = 0;
            pool->count--;
            wgpuBufferRelease(pool->buffer[pool->count]);
            pool->buffer[pool->count] = NULL;
            pool->data[pool->count] = NULL;

            DEBUG_PRINT(DEBUG_PRINT_GENERAL, "staging buffers: released unused buffer (%u in pool) with size %u\n", pool->count, pool->num_bytes);
        }
    }
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);
    //DEBUG_PRINT(DEBUG_PRINT_GENERAL, "staging buffers: selected buffer (%u in pool) with size %u\n", pool->cur, pool->num_bytes);
}

void _spUpdateUBODynamic(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.dynamic_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolDynamicCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->data[pool->cur]);

    for(uint32_t i = 1; i < _sp_state.pools.instance_pool.last_index_plus_1; i++) {
        SPMeshID mesh_id = _sp_state.pools.instances[i].mesh;
        SPMaterialID mat_id = _sp_state.pools.instances[i].material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        SPInstance* instance = &(_sp_state.pools.instances[i]);
        _SPUboDynamic ubo = {
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

        uint64_t offset = (i - 1) * _sp_state.dynamic_alignment;

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &ubo, sizeof(_SPUboDynamic));
    }
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdate_SPUBOCommon(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.common_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolCommonCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);

    _SPUboCommon ubo = {
        .view = GLM_MAT4_IDENTITY_INIT,
        .proj = GLM_MAT4_IDENTITY_INIT,
    };
    memcpy(ubo.view, _sp_state.active_cam._view, sizeof(mat4));
    memcpy(ubo.proj, _sp_state.active_cam._proj, sizeof(mat4));

    memcpy(pool->data[pool->cur], &ubo, sizeof(_SPUboCommon));

    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdateUBOMaterial(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.material_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolMaterialCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);
    
    for(uint32_t i = 1; i < _sp_state.pools.material_pool.last_index_plus_1; i++) {
        SPMaterial* material = &(_sp_state.pools.materials[i]);
        uint64_t offset = (i - 1) * _sp_state.dynamic_alignment;

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &(material->props), sizeof(SPMaterialProperties));
        
    }
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdate(void) {
    _spUpdateView();
    _spUpdateProjection();
    _spUpdate_SPUBOCommon();
    _spUpdateUBODynamic();
    _spUpdateUBOMaterial();
}


void _spSubmit(void) {
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(_sp_state.cmd_enc, NULL);
    wgpuCommandEncoderRelease(_sp_state.cmd_enc);
    _sp_state.cmd_enc = NULL;

    wgpuQueueSubmit(_sp_state.queue, 1, &commands);
    wgpuCommandBufferRelease(commands);

    _sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
}

void _spSortInstances(void) {
    uint32_t materials_count = _sp_state.pools.material_pool.size - 1;
    memset(_sp_state.instance_counts_per_mat, 0, sizeof(uint32_t) * materials_count);
    //memset(_sp_state.sorted_instances, 0, sizeof(SPInstanceID) * (_sp_state.pools.instance_pool.size - 1) * materials_size);
    for(uint32_t i = 1; i < _sp_state.pools.instance_pool.size; i++) {
        SPInstance* instance = &(_sp_state.pools.instances[i]);
        SPMeshID mesh_id = instance->mesh;
        SPMaterialID mat_id = instance->material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        _sp_state.sorted_instances[(mat_id.id - 1) * (_sp_state.pools.instance_pool.size - 1) + _sp_state.instance_counts_per_mat[mat_id.id - 1]++] = (SPInstanceID){i};
    }
}

void _spDrawAllInstances(WGPURenderPassEncoder render_pass_enc) {
    SPMeshID last_mesh_id = {0};
    for(uint32_t ins_id = 1; ins_id < _sp_state.pools.instance_pool.size; ins_id++) {
        SPInstance* instance = &(_sp_state.pools.instances[ins_id]);
        SPMeshID mesh_id = instance->mesh;
        if(mesh_id.id == SP_INVALID_ID) {
            continue;
        }
        SPMesh* mesh = &(_sp_state.pools.meshes[mesh_id.id]);
        if(last_mesh_id.id != mesh_id.id) {
            wgpuRenderPassEncoderSetVertexBuffer(render_pass_enc, 0, mesh->vertex_buffer, 0);
            wgpuRenderPassEncoderSetIndexBuffer(render_pass_enc, mesh->index_buffer, 0);
            last_mesh_id = mesh_id;
        }
        uint32_t offsets_vert[] = { (ins_id - 1) * _sp_state.dynamic_alignment};
        wgpuRenderPassEncoderSetBindGroup(render_pass_enc, 0, _sp_state.shadow_dir_bind_group, ARRAY_LEN(offsets_vert), offsets_vert);
        wgpuRenderPassEncoderDrawIndexed(render_pass_enc, mesh->indices_count, 1, 0, 0, 0);
    }
}

#endif // SPIDER_H_