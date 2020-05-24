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

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define SPIDER_ASSERT(assertion) assert(assertion)
#define SPIDER_MALLOC(size) malloc(size)
#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])

#define DEBUG_PRINT_ALLOWED 1
#define DEBUG_PRINT_TYPE_INIT 0
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 0
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 0
#define DEBUG_PRINT_MESH 0
#define DEBUG_PRINT_RENDER 0
#define DEBUG_PRINT_METRICS 0
#define DEBUG_PRINT_GLTF_LOAD 1

#define _SP_RELEASE_RESOURCE(Type, Name) if(Name) {wgpu##Type##Release(Name); Name = NULL;}

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

typedef struct SPMeshInitializer {
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
} SPMeshInitializer;

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
    float far;  // not used with infinite far plane
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
        uint32_t materials;
        uint32_t meshes;
        uint32_t instances;
        uint32_t lights;
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

typedef struct _SPComputePipeline {
    _SPShaderStage shader;
    WGPUBindGroup bind_group;
    WGPUComputePipeline pipeline;
} _SPComputePipeline;

typedef struct _SPMaterialTexture {
    WGPUTexture texture;
    WGPUTextureView view;
} _SPMaterialTexture;

typedef struct SPMaterialProperties {
    float roughness;
    float metallic;
    float ao;
} SPMaterialProperties;

typedef struct _SPUboMaterialProperties {
    vec4 rmap; // roughness, metallic, ao, padding
} _SPUboMaterialProperties;

typedef struct SPMaterial {
    struct {
        WGPUBindGroup vert;
        WGPUBindGroup frag;
    } bind_groups;
    SPMaterialProperties props;
    _SPMaterialTexture albedo;
    _SPMaterialTexture normal;
    _SPMaterialTexture ao_roughness_metallic;
    WGPUSampler sampler;
} SPMaterial;

typedef struct SPMaterialDesc {
    const char* albedo;
    const char* normal;
    const char* ao_roughness_metallic;
} SPMaterialDesc;

typedef struct SPMaterialID {
    uint32_t id;
} SPMaterialID;

typedef struct SPObject {
    SPMeshID mesh;
    SPMaterialID material;
} SPObject;

typedef enum SPLightType {
    SPLightType_Directional = 0,
    SPLightType_Spot = 1,
    SPLightType_Point = 2,
    SPLightType_Force32 = 0x7FFFFFFF,
} SPLightType;

typedef struct SPColorRGB8 {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t v[3];
    };
} SPColorRGB8;

typedef struct SPLight {
    SPLightType type;
    mat4 view;
    mat4 proj;
    vec3 pos;
    float range;
    SPColorRGB8 color;
    vec3 dir; // for spot & dir
    float fov; // for spot
    vec2 area; // for dir
    float power;
    WGPUTexture depth_texture;
    WGPUTextureView depth_view;
    WGPUTexture color_texture;
    WGPUTextureView color_view;
} SPLight;

typedef struct SPLightShadowCastDesc {
    uint32_t shadow_map_size;
} SPLightShadowCastDesc;

typedef struct SPSpotLightDesc {
    vec3 pos;
    float range;
    SPColorRGB8 color;
    vec3 dir;
    float fov;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPSpotLightDesc;

typedef struct SPDirectionalLightDesc {
    vec3 pos;
    float range;
    vec3 color;
    vec3 dir;
    vec2 area;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPDirectionalLightDesc;

typedef struct SPPointLightDesc {
    vec3 pos;
    float range;
    vec3 color;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPPointLightDesc;

typedef struct SPLightID {
    uint32_t id;
} SPLightID;

typedef struct _SPUboCamera {
    mat4 view;
    mat4 proj;
    vec3 pos;
} _SPUboCamera;

typedef struct _SPUboModel {
    mat4 model;
} _SPUboModel;

typedef struct _SPUboLight {
    mat4 view; // 64 - 4 blocks
    mat4 proj; // 64 - 4 blocks
    vec4 pos3_range1; // 16
    vec4 color3_type1; // 16
    vec4 dir3_fov1; // for spot & dir - 16
    vec4 area2_power1_padding1; // for dir - 16
} _SPUboLight; // 192 bytes

typedef struct SPTransform {
    vec3 pos; // 12 bytes
    vec3 rot; // 12 bytes
    vec3 scale; // 12 bytes
} SPTransform; // 36 bytes

typedef struct SPInstance {
    SPObject object;
    SPTransform transform;
} SPInstance;

typedef struct SPInstanceDesc {
    SPObject object;
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

    _SPPool light_pool;
    SPLight* lights;
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
        WGPUBuffer camera;
        _SPStagingBufferPool camera_staging;
        WGPUBuffer model;
        _SPStagingBufferPool model_staging;
        WGPUBuffer material;
        _SPStagingBufferPool material_staging;
        WGPUBuffer light;
        _SPStagingBufferPool light_staging;
    } uniform;
} _SPBuffers;    

#define _SP_GET_DEFAULT_IF_ZERO(value, default_value) value ? value : default_value 

#define _SP_MATERIAL_POOL_MAX 8
#define _SP_MESH_POOL_MAX 256
#define _SP_INSTANCE_POOL_MAX 256
#define _SP_LIGHT_POOL_MAX 8

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
        struct {
            _SPComputePipeline mipmaps;
        } compute;
    } pipelines;

    uint32_t* instance_counts_per_mat;
    SPInstanceID* sorted_instances;
    
    // TODO: move (but don't know where yet)
    WGPUBindGroup light_bind_group;

    struct {
        _SPMaterialTexture normal;
        _SPMaterialTexture ao_roughness_metallic;
        _SPMaterialTexture ao;
    } default_textures;

} _SPState;
_SPState _sp_state;


typedef struct _SPFileReadResult {
    uint32_t size;
    char* data;
} _SPFileReadResult;

typedef struct _SPTextureViewFromImageDescriptor {
    _SPMaterialTexture* mat_tex;
    const char* file;
    uint8_t channel_count;
    WGPUTextureViewDescriptor* tex_view_desc;
} _SPTextureViewFromImageDescriptor;

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
SPMeshID spCreateMeshFromInit(const SPMeshInitializer* init);
/*
Creates a material and returns an identifier to it
*/
SPMaterialID spCreateMaterial(const SPMaterialDesc* desc);
/*
Creates an instance and returns an identifier to it
*/
SPInstanceID spCreateInstance(const SPInstanceDesc* desc);
/*
Creates a light of type 'spot' and returns an identifier to it
*/
SPLightID spCreateSpotLight(const SPSpotLightDesc* desc);
/*
Creates a light of type 'directional' and returns an identifier to it
*/
SPLightID spCreateDirectionalLight(const SPDirectionalLightDesc* desc);
/*
Creates a light of type 'point' and returns an identifier to it
*/
SPLightID spCreatePointLight(const SPPointLightDesc* desc);
/*
Loads a mesh from a .gltf file, creates a mesh from the data and returns an identifier to it
*/
SPObject spLoadGltf(const char* file);

/*
Returns a temporary pointer to the active camera 
*/
SPCamera* spGetActiveCamera();
/*
Returns a temporary pointer to the instance with the specified id
NULL if not a valid id
*/
SPInstance* spGetInstance(SPInstanceID instance_id);
/*
Returns a temporary pointer to the light with the specified id
NULL if not a valid id
*/
SPLight* spGetLight(SPLightID light_id);

// PRIVATE
/*
Creates the forward render pipeline with all it's subresources
*/
void _spCreateForwardRenderPipeline();
/*
Creates the shadow map render pipeline with all it's subresources
*/
void _spCreateShadowMapRenderPipeline();
/*
Creates the mipmaps compute pipeline with all it's subresources
*/
void _spCreateMipmapsComputePipeline();

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
foreach desc in descriptors:
- Loads an image with desc.tex_file_desc info
- Creates a WGPUTexture object and stores it in desc.mat_tex->texture
- Creates a WGPUTextureView object and stores it in desc.mat_tex->view
- Uploads image data to texture via a temporary buffer and copyBufferToTexture
*/
void _spCreateAndLoadTextures(_SPTextureViewFromImageDescriptor descriptors[], const size_t count);

// Matrices
/* Updates the view matrix */
void _spUpdateView(void);
/* Updates the projection matrix */
void _spUpdateProjection(void);

// Staging buffer pools
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing dynamic model data */
void _spUpdateStagingPoolDynamicCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing common data */
void _spUpdateStagingPoolCameraCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing material data */
void _spUpdateStagingPoolMaterialCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
/* Callback for wgpuBufferMapWriteAsync for the staging buffer pool containing light data */
void _spUpdateStagingPoolLightCallback(WGPUBufferMapAsyncStatus status, void* data, uint64_t dataLength, void * userdata);
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
and copies them to the current mapped 'model' staging buffer */
void _spUpdateUboModel(void);
/* Copies the view and projection matrices to the current mapped 'camera' staging buffer  */
void _spUpdateUboCamera(void);
/* Copies the material properties for each material to the current mapped 'material' staging buffer  */
void _spUpdateUboMaterial(void);
/* Copies the light properties for each light (currently just one) to the current mapped 'light' staging buffer  */
void _spUpdateUboLight(void);

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

/*
Creates a list for each material with all the instances using the material
*/
void _spSortInstances(void);
/*
Converts a 8-byte uint color component to an 32-bit float color component
*/ 
float _spColorComponent8ToFloat(uint8_t component);

void _spReadFile(const char* filename, _SPFileReadResult* result);

/*
Creates a perspective matrix [near = 1, far = 0] 
http://dev.theomader.com/depth-precision/
*/
void _spPerspectiveMatrixReversedZ(float fovy, float aspect, float near, float far, mat4 dest);

/*
Creates a perspective matrix [near = 1, infinite = 0] without far plane
http://dev.theomader.com/depth-precision/
*/
void _spPerspectiveMatrixReversedZInfiniteFar(float fovy, float aspect, float near, mat4 dest);

/*
Creates a mesh from the loaded gltf file (it loads the buffers) and returns an ID to it
*/
SPMeshID _spLoadMeshFromGltf(cgltf_data* data, const char* gltf_path);
/*
Creates a material from the loaded gltf file (it loads the images) and returns an ID to it
*/
SPMaterialID _spLoadMaterialFromGltf(const cgltf_data* data, const char* gltf_path);

/*
Combines two paths and saves it in result (used for loading gltf materials that store images relative to the base file)
Example:
base: foo/bar/xyz.txt
new: abc.bin
result: foo/bar/abc.bin
*/
void _spModifyRelativeFilePath(const char* base_path, const char* new_path, char* result);

// IMPLEMENTATION

// PUBLIC
void spInit(const SPInitDesc* desc) {
    _sp_state.device = emscripten_webgpu_get_device();
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: got device\n");
    wgpuDeviceSetUncapturedErrorCallback(_sp_state.device, &errorCallback, NULL);

    _sp_state.queue = wgpuDeviceGetDefaultQueue(_sp_state.device);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created queue\n");

    WGPUSurfaceDescriptorFromHTMLCanvasId canvas_desc = {
        .chain = {
            .sType = WGPUSType_SurfaceDescriptorFromHTMLCanvasId
        },
        .id = "canvas"
    };

    WGPUSurfaceDescriptor surf_desc = {
        .nextInChain = (WGPUChainedStruct const*)&canvas_desc, 
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
        .presentMode = WGPUPresentMode_Fifo
    };
    _sp_state.swap_chain = wgpuDeviceCreateSwapChain(_sp_state.device, _sp_state.surface, &sc_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created swapChain\n");

    _sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
    
    _spSetupPools(&(_sp_state.pools), &(desc->pools));

    // Camera uniform buffer creation
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = sizeof(_SPUboCamera),
        };

       _sp_state.buffers.uniform.camera = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    
    _sp_state.buffers.uniform.camera_staging.count = 0;
    _sp_state.buffers.uniform.camera_staging.cur = 0;
    _sp_state.buffers.uniform.camera_staging.num_bytes = sizeof(_SPUboCamera);

    _sp_state.dynamic_alignment = 256;

    // Model uniform buffer creation
    const uint32_t instance_count = (_sp_state.pools.instance_pool.size - 1);
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = _sp_state.dynamic_alignment * instance_count,
        };

        _sp_state.buffers.uniform.model = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    _sp_state.buffers.uniform.model_staging.count = 0;
    _sp_state.buffers.uniform.model_staging.cur = 0;
    _sp_state.buffers.uniform.model_staging.num_bytes = _sp_state.dynamic_alignment * instance_count;

    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created uniform_buffer with size %u\n", _sp_state.buffers.uniform.model_staging.num_bytes);

    // Light uniform buffer creation
    const uint32_t light_count = (_sp_state.pools.light_pool.size - 1);
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = _sp_state.dynamic_alignment * light_count,
        };

        _sp_state.buffers.uniform.light = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }

    _sp_state.buffers.uniform.light_staging.count = 0;
    _sp_state.buffers.uniform.light_staging.cur = 0;
    _sp_state.buffers.uniform.light_staging.num_bytes = _sp_state.dynamic_alignment * light_count;
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created uniform_buffer with size %u\n", _sp_state.buffers.uniform.light_staging.num_bytes);

    _sp_state.active_cam = desc->camera;

    _spCreateForwardRenderPipeline();
    _spCreateShadowMapRenderPipeline();

    _sp_state.instance_counts_per_mat = SPIDER_MALLOC(sizeof(uint32_t) * desc->pools.capacities.materials);
    _sp_state.sorted_instances = SPIDER_MALLOC((sizeof(SPInstanceID) * desc->pools.capacities.materials) * desc->pools.capacities.instances);

    WGPUTextureViewDescriptor tex_view_desc_linear_32 = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 0,
        .baseArrayLayer = 0,
        .arrayLayerCount = 0,
        .aspect = WGPUTextureAspect_All,
    };

    _SPTextureViewFromImageDescriptor default_image_descs[] = {
        {
            &(_sp_state.default_textures.normal),
            "assets/textures/default_normal_32.png",
            4,
            &tex_view_desc_linear_32
        },
        {
            &(_sp_state.default_textures.ao_roughness_metallic),
            "assets/textures/default_ao_roughness_metallic_32.png",
            4,
            &tex_view_desc_linear_32
        }
    };

    _spCreateAndLoadTextures(default_image_descs, ARRAY_LEN(default_image_descs));
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "init: created default texture views\n");
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
        _SP_RELEASE_RESOURCE(Sampler, material->sampler)
        _SP_RELEASE_RESOURCE(Texture, material->albedo.texture)
        _SP_RELEASE_RESOURCE(TextureView, material->albedo.view)
        _SP_RELEASE_RESOURCE(Texture, material->ao_roughness_metallic.texture)
        _SP_RELEASE_RESOURCE(TextureView, material->ao_roughness_metallic.view)
    }
    _spDiscardPool(&_sp_state.pools.material_pool);
    free(_sp_state.pools.materials);
    _sp_state.pools.materials = NULL;

    // Instances don't have resources that need to be released/freed
    _spDiscardPool(&_sp_state.pools.instance_pool);
    free(_sp_state.pools.instances);
    _sp_state.pools.instances = NULL;

    // Lights don't have resources that need to be released/freed
    _spDiscardPool(&_sp_state.pools.light_pool);
    free(_sp_state.pools.lights);
    _sp_state.pools.lights = NULL;

    _spDiscardStagingBuffers();
    wgpuBufferRelease(_sp_state.buffers.uniform.camera);
    wgpuBufferRelease(_sp_state.buffers.uniform.model);
    wgpuBufferRelease(_sp_state.buffers.uniform.material);
    wgpuBufferRelease(_sp_state.buffers.uniform.light);

    free(_sp_state.sorted_instances);
    _sp_state.sorted_instances = NULL;
    free(_sp_state.instance_counts_per_mat);
    _sp_state.instance_counts_per_mat = NULL;

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

    _SP_RELEASE_RESOURCE(BindGroup, _sp_state.light_bind_group)

}

void spUpdate(void) {
    _spUpdate();
}


void spRender(void) {
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: start\n");
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.camera_staging.buffer[_sp_state.buffers.uniform.camera_staging.cur], 0, 
        _sp_state.buffers.uniform.camera, 0, 
        _sp_state.buffers.uniform.camera_staging.num_bytes
    ); 
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy common ubo\n");

    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.model_staging.buffer[_sp_state.buffers.uniform.model_staging.cur], 0, 
        _sp_state.buffers.uniform.model, 0, 
        _sp_state.buffers.uniform.model_staging.num_bytes
    );
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy dynamic ubo\n");

    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc, 
        _sp_state.buffers.uniform.light_staging.buffer[_sp_state.buffers.uniform.light_staging.cur], 0, 
        _sp_state.buffers.uniform.light, 0, 
        _sp_state.buffers.uniform.light_staging.num_bytes
    );
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: recorded copy light ubo\n");

    WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);
    
    _spSortInstances();

    // shadow maps
    // ***
    {
        SPLight* light = &(_sp_state.pools.lights[1]); // TODO: currently just 1 light supported
        WGPURenderPassColorAttachmentDescriptor color_attachment = {
            .attachment = light->color_view,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearColor = (WGPUColor){0.0f, 0.0f, 0.0, 1.0f}
        };

        WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
            .attachment = light->depth_view,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .clearDepth = 0.0f,
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
        SPMeshID last_mesh_id = {0};
        for(uint32_t ins_id = 1; ins_id < _sp_state.pools.instance_pool.size; ins_id++) {
            SPInstance* instance = &(_sp_state.pools.instances[ins_id]);
            SPMeshID mesh_id = instance->object.mesh;
            if(mesh_id.id == SP_INVALID_ID) {
                continue;
            }
            SPMesh* mesh = &(_sp_state.pools.meshes[mesh_id.id]);
            if(last_mesh_id.id != mesh_id.id) {
                wgpuRenderPassEncoderSetVertexBuffer(shadow_pass_enc, 0, mesh->vertex_buffer, 0, 0);
                wgpuRenderPassEncoderSetIndexBuffer(shadow_pass_enc, mesh->index_buffer, 0, 0);
                last_mesh_id = mesh_id;
            }
            uint32_t offsets_vert[] = { (ins_id - 1) * _sp_state.dynamic_alignment};
            wgpuRenderPassEncoderSetBindGroup(shadow_pass_enc, 0, _sp_state.light_bind_group, ARRAY_LEN(offsets_vert), offsets_vert);
            wgpuRenderPassEncoderDrawIndexed(shadow_pass_enc, mesh->indices_count, 1, 0, 0, 0);
        }
        wgpuRenderPassEncoderEndPass(shadow_pass_enc);
        wgpuRenderPassEncoderRelease(shadow_pass_enc);
        DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
        _spSubmit();
    }
    // ***

    // main pass
    // ***
    #define FORWARD_OPAQUE_PASS 1
    #if FORWARD_OPAQUE_PASS
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
            .clearDepth = 0.0f,
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
            //uint32_t offsets_frag[] = { (mat_id - 1) * _sp_state.dynamic_alignment};
            wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 1, material->bind_groups.frag, 0, NULL /*ARRAY_LEN(offsets_frag), offsets_frag*/);
            SPMeshID last_mesh_id = {0};
            uint32_t instance_count = _sp_state.instance_counts_per_mat[mat_id - 1];
            for(uint32_t i = 0; i < instance_count; i++) {
                SPInstanceID ins_id = _sp_state.sorted_instances[(mat_id - 1) * (_sp_state.pools.instance_pool.size - 1) + i];
                SPInstance* instance = &(_sp_state.pools.instances[ins_id.id]);
                SPMeshID mesh_id = instance->object.mesh;
                SPMesh* mesh = &(_sp_state.pools.meshes[mesh_id.id]);
                if(last_mesh_id.id != mesh_id.id) {
                    wgpuRenderPassEncoderSetVertexBuffer(_sp_state.pass_enc, 0, mesh->vertex_buffer, 0, 0);
                    wgpuRenderPassEncoderSetIndexBuffer(_sp_state.pass_enc, mesh->index_buffer, 0, 0);
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

SPMaterialID spCreateMaterial(const SPMaterialDesc* desc) {
    SPMaterialID material_id = (SPMaterialID){_spAllocPoolIndex(&(_sp_state.pools.material_pool))};
    if(material_id.id == SP_INVALID_ID) {
        return material_id;
    }
    int id = material_id.id; 
    SPMaterial* material = &(_sp_state.pools.materials[id]);

    WGPUTextureViewDescriptor tex_view_desc_srgb_32 = {
        .format = WGPUTextureFormat_RGBA8UnormSrgb,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 0,
        .baseArrayLayer = 0,
        .arrayLayerCount = 0,
        .aspect = WGPUTextureAspect_All,
    };
    WGPUTextureViewDescriptor tex_view_desc_linear_32 = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 0,
        .baseArrayLayer = 0,
        .arrayLayerCount = 0,
        .aspect = WGPUTextureAspect_All,
    };

    _SPTextureViewFromImageDescriptor image_descs[] = {
        {
            &(material->albedo),
            desc->albedo,
            4,
            &tex_view_desc_srgb_32
        },
        {
            &(material->normal),
            desc->normal,
            4,
            &tex_view_desc_linear_32
        },
        {
            &(material->ao_roughness_metallic),
            desc->ao_roughness_metallic,
            4,
            &tex_view_desc_linear_32
        }
    };

    _spCreateAndLoadTextures(image_descs, ARRAY_LEN(image_descs));

    WGPUSamplerDescriptor sampler_desc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Linear,
        .mipmapFilter = WGPUFilterMode_Linear,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 1.0f,
        .compare = WGPUCompareFunction_Undefined,
    };

    material->sampler = wgpuDeviceCreateSampler(_sp_state.device, &sampler_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created sampler\n");
    
    WGPUBindGroupEntry vert_bindings[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.model,
            .offset = 0,
            .size = sizeof(_SPUboModel),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = _sp_state.buffers.uniform.camera,
            .offset = 0,
            .size = sizeof(_SPUboCamera),
            .sampler = NULL,
            .textureView = NULL,
        }
    };

    WGPUBindGroupDescriptor vert_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.vert.bind_group_layout,
        .entryCount = ARRAY_LEN(vert_bindings),
        .entries = vert_bindings
    };
    material->bind_groups.vert = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created vert bind group\n");
    SPIDER_ASSERT(_sp_state.pools.lights[1].color_view);
    
    WGPUBindGroupEntry frag_bindings[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.light,
            .offset = 0,
            .size = sizeof(_SPUboLight),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = material->sampler,
            .textureView = NULL,
        },
        {
            .binding = 2,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = material->albedo.view,
        },
        {
            .binding = 3,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = material->normal.view ? material->normal.view :_sp_state.default_textures.normal.view,
        },
        {
            .binding = 4,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = material->ao_roughness_metallic.view ? material->ao_roughness_metallic.view :_sp_state.default_textures.ao_roughness_metallic.view,
        },
        {
            .binding = 5,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = _sp_state.pools.lights[1].color_view, // TODO: currently just 1 light supported
        },
    };
    WGPUBindGroupDescriptor frag_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.frag.bind_group_layout,
        .entryCount = ARRAY_LEN(frag_bindings),
        .entries = frag_bindings
    };
    material->bind_groups.frag = wgpuDeviceCreateBindGroup(_sp_state.device, &frag_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created frag bind group\n");

    return material_id;
}

SPInstanceID spCreateInstance(const SPInstanceDesc* desc) {
    SPIDER_ASSERT(desc->object.mesh.id != SP_INVALID_ID && desc->object.material.id != SP_INVALID_ID);
    if(!desc->object.mesh.id || !desc->object.material.id) {
        return (SPInstanceID){SP_INVALID_ID};
    }
    SPInstanceID instance_id = (SPInstanceID){_spAllocPoolIndex(&(_sp_state.pools.instance_pool))};
    if(instance_id.id == SP_INVALID_ID) {
        return instance_id;
    }
    int id = instance_id.id; 
    SPInstance* instance = &(_sp_state.pools.instances[id]);
    instance->object.mesh = desc->object.mesh;
    instance->object.material = desc->object.material;
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

SPLightID spCreateSpotLight(const SPSpotLightDesc* desc){
    DEBUG_PRINT(DEBUG_PRINT_GENERAL, "create light: start\n");
    SPLightID light_id = (SPLightID){_spAllocPoolIndex(&(_sp_state.pools.light_pool))};
    if(light_id.id == SP_INVALID_ID) {
        return light_id;
    }
    int id = light_id.id; 
    SPLight* light = &(_sp_state.pools.lights[id]);

    light->type = SPLightType_Spot;
    memcpy(light->view, GLM_MAT4_IDENTITY, sizeof(mat4));
    memcpy(light->proj, GLM_MAT4_IDENTITY, sizeof(mat4));
    memcpy(light->pos, desc->pos, sizeof(vec3));
    light->range = desc->range;
    light->color = desc->color;
    memcpy(light->dir, desc->dir, sizeof(vec3));
    light->fov = desc->fov;
    light->power = desc->power;
    if(desc->shadow_casting) {
        DEBUG_PRINT(DEBUG_PRINT_GENERAL, "create light: has shadow casting\n");
        WGPUExtent3D texture_size = {
            .width = desc->shadow_casting->shadow_map_size,
            .height = desc->shadow_casting->shadow_map_size,
            .depth = 1,
        };

        WGPUTextureDescriptor depth_tex_desc = {
            .usage = WGPUTextureUsage_OutputAttachment, // TODO: should sample directly from depth texture
            .dimension = WGPUTextureDimension_2D,
            .size = texture_size,
            .arrayLayerCount = texture_size.depth, // TODO: deprecated, but needed for dawn
            .format = WGPUTextureFormat_Depth32Float,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

        light->depth_texture = wgpuDeviceCreateTexture(_sp_state.device, &depth_tex_desc);

        WGPUTextureViewDescriptor depth_tex_view_desc = {
            .format = WGPUTextureFormat_Depth32Float,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 0,
            .baseArrayLayer = 0,
            .arrayLayerCount = 0,
            .aspect = WGPUTextureAspect_All,
        };

        light->depth_view = wgpuTextureCreateView(light->depth_texture, &depth_tex_view_desc);

        /*
        It's apparently not possible to sample from a depth texture in WebGPU yet
        So we have to create an extra color texture which copies the depth information
        */
        WGPUTextureDescriptor color_tex_desc = {
            .usage = WGPUTextureUsage_Sampled | WGPUTextureUsage_OutputAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = texture_size,
            .arrayLayerCount = texture_size.depth, // TODO: deprecated, but needed for dawn
            .format = WGPUTextureFormat_R32Float,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

        light->color_texture = wgpuDeviceCreateTexture(_sp_state.device, &color_tex_desc);

        WGPUTextureViewDescriptor color_tex_view_desc = {
            .format = WGPUTextureFormat_R32Float,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 0,
            .baseArrayLayer = 0,
            .arrayLayerCount = 0,
            .aspect = WGPUTextureAspect_All,
        };

        light->color_view = wgpuTextureCreateView(light->color_texture, &color_tex_view_desc);
    }
    return light_id;
}

// TODO: not implemented yet
SPLightID spCreateDirectionalLight(const SPDirectionalLightDesc* desc) {
    return (SPLightID){0};
}

// TODO: not implemented yet
SPLightID spCreatePointLight(const SPPointLightDesc* desc) {
    return (SPLightID){0};
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

SPLight* spGetLight(SPLightID light_id) {
    if(light_id.id == SP_INVALID_ID || light_id.id >= _sp_state.pools.light_pool.size) {
        return NULL;
    }
    return &(_sp_state.pools.lights[light_id.id]);
}


// PRIVATE

void _spCreateForwardRenderPipeline() {
    WGPUTextureDescriptor texture_desc = {
        .usage = WGPUTextureUsage_OutputAttachment,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = _sp_state.surface_size.width,
            .height = _sp_state.surface_size.height,
            .depth = 1,
        },
        .arrayLayerCount = 1, // TODO: deprecated, but needed for dawn
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
        _SPFileReadResult vertShader;
        _spReadFile("src/shaders/compiled/forward.vert.spv", &vertShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", vertShader.size);
        WGPUShaderModuleSPIRVDescriptor sm_desc = {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
            },
            .codeSize = vertShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)vertShader.data
        };
        WGPUShaderModuleDescriptor sm_wrapper = {
            .nextInChain = (WGPUChainedStruct const*)&sm_desc,
        };
        pipeline->vert.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_wrapper);
    }
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert shader\n");
    {
        _SPFileReadResult fragShader;
        _spReadFile("src/shaders/compiled/forward_pbr.frag.spv", &fragShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", fragShader.size);
        WGPUShaderModuleSPIRVDescriptor sm_desc = {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
            },
            .codeSize = fragShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)fragShader.data
        };
        WGPUShaderModuleDescriptor sm_wrapper = {
            .nextInChain = (WGPUChainedStruct const*)&sm_desc,
        };
        pipeline->frag.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_wrapper);
    }
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created frag shader\n");

    WGPUBindGroupLayoutEntry vert_bgles[] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = true,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        }
    };

    WGPUBindGroupLayoutDescriptor vert_bgl_desc = {
        .entryCount = ARRAY_LEN(vert_bgles),
        .entries = vert_bgles
    };
    pipeline->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &vert_bgl_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert bind group layout\n");
    
    WGPUBindGroupLayoutEntry frag_bgles[] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            // texture sampler (also used for shadow map)
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_Sampler,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_2D,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            // base color
            .binding = 2,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_SampledTexture,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_2D,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            // normal
            .binding = 3,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_SampledTexture,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_2D,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            // ao_metallic_roughness
            .binding = 4,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_SampledTexture,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_2D,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            // shadow map
            .binding = 5,
            .visibility = WGPUShaderStage_Fragment,
            .type = WGPUBindingType_SampledTexture,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_2D,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
    };

    WGPUBindGroupLayoutDescriptor frag_bgl_desc = {
        .entryCount = ARRAY_LEN(frag_bgles),
        .entries = frag_bgles
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
            .format = WGPUVertexFormat_Float2,
            .offset = offsetof(SPVertex, tex_coords),
            .shaderLocation = 1
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(SPVertex, normal),
            .shaderLocation = 2
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(SPVertex, tangent),
            .shaderLocation = 3
        },
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
        .depthCompare = WGPUCompareFunction_Greater,
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
}

void _spCreateShadowMapRenderPipeline() {
    _SPRenderPipeline* pipeline = &(_sp_state.pipelines.render.shadow);
    {
        _SPFileReadResult vertShader;
        _spReadFile("src/shaders/compiled/shadow.vert.spv", &vertShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", vertShader.size);
        WGPUShaderModuleSPIRVDescriptor sm_desc = {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
            },
            .codeSize = vertShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)vertShader.data
        };
        WGPUShaderModuleDescriptor sm_wrapper = {
            .nextInChain = (WGPUChainedStruct const*)&sm_desc,
        };
        pipeline->vert.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_wrapper);
    }
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow: created vert shader\n");

    {
        _SPFileReadResult fragShader;
        _spReadFile("src/shaders/compiled/shadow.frag.spv", &fragShader);
        DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "read file: size: %d\n", fragShader.size);
        WGPUShaderModuleSPIRVDescriptor sm_desc = {
            .chain = {
                .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
            },
            .codeSize = fragShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)fragShader.data
        };
        WGPUShaderModuleDescriptor sm_wrapper = {
            .nextInChain = (WGPUChainedStruct const*)&sm_desc,
        };
        pipeline->frag.module = wgpuDeviceCreateShaderModule(_sp_state.device, &sm_wrapper);
    }
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow: created frag shader\n");

    WGPUBindGroupLayoutEntry vert_bgles[] = {
        {
            .binding = 0,
            .visibility = WGPUShaderStage_Vertex,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = true,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Vertex,
            .type = WGPUBindingType_UniformBuffer,
            .hasDynamicOffset = false,
            .multisampled = false,
            .viewDimension = WGPUTextureViewDimension_Undefined,
            .textureComponentType = WGPUTextureComponentType_Float,
        },
    };

    WGPUBindGroupLayoutDescriptor vert_bgl_desc = {
        .entryCount = ARRAY_LEN(vert_bgles),
        .entries = vert_bgles
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

    WGPUBindGroupEntry vert_entries[] = {
        {
            .binding = 0,
            .buffer = _sp_state.buffers.uniform.model,
            .offset = 0,
            .size = sizeof(_SPUboModel),
            .sampler = NULL,
            .textureView = NULL,
        },
        {
            .binding = 1,
            .buffer = _sp_state.buffers.uniform.light,
            .offset = 0,
            .size = sizeof(_SPUboLight),
            .sampler = NULL,
            .textureView = NULL,
        },
    };

    WGPUBindGroupDescriptor vert_bg_desc = {
        .layout = pipeline->vert.bind_group_layout,
        .entryCount = ARRAY_LEN(vert_entries),
        .entries = vert_entries
    };
    _sp_state.light_bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);

    WGPUVertexAttributeDescriptor vertex_attribute_descs[] = {
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(SPVertex, pos),
            .shaderLocation = 0
        },
        {
            .format = WGPUVertexFormat_Float2,
            .offset = offsetof(SPVertex, tex_coords),
            .shaderLocation = 1
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(SPVertex, normal),
            .shaderLocation = 2
        },
        {
            .format = WGPUVertexFormat_Float3,
            .offset = offsetof(SPVertex, tangent),
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
        .depthCompare = WGPUCompareFunction_Greater,
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
        .frontFace = WGPUFrontFace_CW,
        .cullMode = WGPUCullMode_Front,
        .depthBias = 1, // Depth bias is not yet implemented in Chromium/dawn
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
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created shadow dir pipeline\n");
}

void _spCreateMipmapsComputePipeline() {

}

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

    _spInitPool(&(_sp_state.pools.instance_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.instances, _SP_INSTANCE_POOL_MAX));
    size_t instance_pool_byte_size = sizeof(SPInstance) * pools->instance_pool.size;
    pools->instances = (SPInstance*) SPIDER_MALLOC(instance_pool_byte_size);
    SPIDER_ASSERT(pools->instances);
    memset(pools->instances, 0, instance_pool_byte_size);

    _spInitPool(&(_sp_state.pools.light_pool), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.lights, _SP_LIGHT_POOL_MAX));
    size_t light_pool_byte_size = sizeof(SPLight) * pools->light_pool.size;
    pools->lights = (SPLight*) SPIDER_MALLOC(light_pool_byte_size);
    SPIDER_ASSERT(pools->lights);
    memset(pools->lights, 0, light_pool_byte_size);
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


void _spCreateAndLoadTextures(_SPTextureViewFromImageDescriptor descriptors[], const size_t count) {
    if(count == 0) {
        return;
    }
    WGPUBuffer buffers[count];
    WGPUCommandEncoder texture_load_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
    for(size_t i = 0; i < count; i++) {
        buffers[i] = NULL;
        _SPMaterialTexture* mat_tex = descriptors[i].mat_tex;
        const char* file = descriptors[i].file;
        if(!file) {
            continue;
        }
        WGPUTextureViewDescriptor* tex_view_desc = descriptors[i].tex_view_desc;
        WGPUTextureFormat texture_format = descriptors[i].tex_view_desc->format;

        int width = 0;
        int height = 0;
        int read_comps = (int)(descriptors[i].channel_count);
        const int comp_map[5] = {
            0,
            1, 
            2,
            4,
            4
        };
        const uint32_t channels[5] = {
            STBI_default, // only used for req_comp
            STBI_grey,
            STBI_grey_alpha,
            STBI_rgb_alpha,
            STBI_rgb_alpha
        };

        stbi_uc* pixel_data = stbi_load(
            file,
            &width,
            &height,
            &read_comps,
            channels[read_comps]
        );
        if(!pixel_data) {
            DEBUG_PRINT(DEBUG_PRINT_WARNING, "Couldn't load '%s'\n", file);
        }
        SPIDER_ASSERT(pixel_data);
        int comps = comp_map[read_comps];
        DEBUG_PRINT(DEBUG_PRINT_GENERAL, "loaded image (%d, %d, %d / %d)\n", width, height, read_comps, comps);

        WGPUExtent3D texture_size = {
            .width = width,
            .height = height,
            .depth = 1,
        };

        WGPUTextureDescriptor tex_desc = {
            .usage = WGPUTextureUsage_Sampled | WGPUTextureUsage_CopyDst,
            .dimension = WGPUTextureDimension_2D,
            .size = texture_size,
            .arrayLayerCount = texture_size.depth, // TODO: deprecated, but needed for dawn
            .format = texture_format,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };

        mat_tex->texture = wgpuDeviceCreateTexture(_sp_state.device, &tex_desc);
        mat_tex->view = wgpuTextureCreateView(mat_tex->texture, tex_view_desc);

        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_CopySrc,
            .size = width * height * comps
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
        memcpy(result.data, pixel_data, result.dataLength);
        stbi_image_free(pixel_data);
        wgpuBufferUnmap(result.buffer);
        buffers[i] = result.buffer;

        WGPUBufferCopyView buffer_copy_view = {
            .buffer = result.buffer,
            .offset = 0,
            .bytesPerRow = width * comps,
            .rowsPerImage = height,
        };

        WGPUTextureCopyView texture_copy_view = {
            .texture = mat_tex->texture,
            .mipLevel = 0,
            .arrayLayer = 0,
            .origin = {0, 0, 0},
        };
        wgpuCommandEncoderCopyBufferToTexture(texture_load_enc, &buffer_copy_view, &texture_copy_view, &texture_size);
    }
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(texture_load_enc, NULL);
   _SP_RELEASE_RESOURCE(CommandEncoder, texture_load_enc)
    wgpuQueueSubmit(_sp_state.queue, 1, &cmd_buffer);
    _SP_RELEASE_RESOURCE(CommandBuffer, cmd_buffer)
    for(size_t i = 0; i < count; i++) {
        _SP_RELEASE_RESOURCE(Buffer, buffers[i]);
    }

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
   _spPerspectiveMatrixReversedZInfiniteFar(
        _sp_state.active_cam.fovy,
        _sp_state.active_cam.aspect,
        _sp_state.active_cam.near,
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

_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.model_staging, Dynamic)
_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.camera_staging, Camera)
_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.material_staging, Material)
_SP_CREATE_STAGING_POOL_CALLBACK_IMPL(_sp_state.buffers.uniform.light_staging, Light)

void _spDiscardStagingBuffers() {
    for(uint8_t i = 0; i < _sp_state.buffers.uniform.camera_staging.count; i++) {
        wgpuBufferRelease(_sp_state.buffers.uniform.camera_staging.buffer[i]);
        _sp_state.buffers.uniform.camera_staging.buffer[i] = NULL;
        _sp_state.buffers.uniform.camera_staging.data[i] = NULL;
    }
    _sp_state.buffers.uniform.camera_staging.count = 0;
    for(uint8_t i = 0; i < _sp_state.buffers.uniform.model_staging.count; i++) {
        wgpuBufferRelease(_sp_state.buffers.uniform.model_staging.buffer[i]);
        _sp_state.buffers.uniform.model_staging.buffer[i] = NULL;
        _sp_state.buffers.uniform.model_staging.data[i] = NULL;
    }
    _sp_state.buffers.uniform.model_staging.count = 0;
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
}

void _spUpdateUboModel(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.model_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolDynamicCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->data[pool->cur]);

    for(uint32_t i = 1; i < _sp_state.pools.instance_pool.last_index_plus_1; i++) {
        SPMeshID mesh_id = _sp_state.pools.instances[i].object.mesh;
        SPMaterialID mat_id = _sp_state.pools.instances[i].object.material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        SPInstance* instance = &(_sp_state.pools.instances[i]);
        _SPUboModel ubo = {
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

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &ubo, sizeof(_SPUboModel));
    }
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdateUboCamera(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.camera_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolCameraCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);

    _SPUboCamera ubo = {
        .view = GLM_MAT4_IDENTITY_INIT,
        .proj = GLM_MAT4_IDENTITY_INIT,
        .pos = {0.0f, 0.0f, 0.0f}
    };
    memcpy(ubo.view, _sp_state.active_cam._view, sizeof(mat4));
    memcpy(ubo.proj, _sp_state.active_cam._proj, sizeof(mat4));
    memcpy(ubo.pos, _sp_state.active_cam.pos, sizeof(vec3));

    memcpy(pool->data[pool->cur], &ubo, sizeof(_SPUboCamera));

    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdateUboMaterial(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.material_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolMaterialCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);
    
    for(uint32_t i = 1; i < _sp_state.pools.material_pool.last_index_plus_1; i++) {
        SPMaterial* material = &(_sp_state.pools.materials[i]);
        uint64_t offset = (i - 1) * _sp_state.dynamic_alignment;
        _SPUboMaterialProperties ubo = {
            .rmap = {
                material->props.roughness,
                material->props.metallic,
                material->props.ao,
                0.0f,
            },
        };

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &(material->props), sizeof(_SPUboMaterialProperties));
        
    }
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdateUboLight(void) {
    _SPStagingBufferPool* pool = &(_sp_state.buffers.uniform.light_staging);
    _spUpdateStagingPool(pool, _spUpdateStagingPoolLightCallback);
    SPIDER_ASSERT(pool->cur < pool->count && pool->buffer[pool->cur] && pool->buffer[pool->cur]);
    
    //for(uint32_t i = 1; i < _sp_state.pools.light_pool.last_index_plus_1; i++) {
        uint32_t i = 1; // only 1 light supported right now
        SPLight* light = &(_sp_state.pools.lights[i]);
        uint64_t offset = (i - 1) * _sp_state.dynamic_alignment;
        
        _SPUboLight ubo = {
            .view = GLM_MAT4_IDENTITY_INIT,
            .proj = GLM_MAT4_IDENTITY_INIT,
            .pos3_range1 = {light->pos[0], light->pos[1], light->pos[2], light->range},
            .color3_type1 = {
                _spColorComponent8ToFloat(light->color.r),
                _spColorComponent8ToFloat(light->color.g),
                _spColorComponent8ToFloat(light->color.b),
                (float)light->type
            },
            .dir3_fov1 = {light->dir[0], light->dir[1], light->dir[2], light->fov},
            .area2_power1_padding1 = {light->area[0], light->area[1], light->power, 0.0f},
        };
        glm_look(
            light->pos,
            light->dir,
            light->dir[1] > -1.0f && light->dir[1] < 1.0f ? (vec3){0.0f, 1.0f, 0.0f} : (vec3){0.0f, 0.0f, 1.0f},
            ubo.view
        );
        _spPerspectiveMatrixReversedZ(light->fov, 1.0, 0.1f, light->range, ubo.proj);

        memcpy((void*)((uint64_t)(pool->data[pool->cur]) + offset), &(ubo), sizeof(_SPUboLight));
    //}
    wgpuBufferUnmap(pool->buffer[pool->cur]);
    pool->data[pool->cur] = NULL;
}

void _spUpdate(void) {
    _spUpdateView();
    _spUpdateProjection();
    _spUpdateUboCamera();
    _spUpdateUboModel();
    _spUpdateUboLight();
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
    for(uint32_t i = 1; i < _sp_state.pools.instance_pool.size; i++) {
        SPInstance* instance = &(_sp_state.pools.instances[i]);
        SPMeshID mesh_id = instance->object.mesh;
        SPMaterialID mat_id = instance->object.material;
        if(mesh_id.id == SP_INVALID_ID || mat_id.id == SP_INVALID_ID) {
            continue;
        }
        _sp_state.sorted_instances[(mat_id.id - 1) * (_sp_state.pools.instance_pool.size - 1) + _sp_state.instance_counts_per_mat[mat_id.id - 1]++] = (SPInstanceID){i};
    }
}

inline float _spColorComponent8ToFloat(uint8_t component) {
    return (float)component / 255.0f;
}

void _spReadFile(const char* filename, _SPFileReadResult* result) {
    FILE *f = fopen(filename, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    result->size = ftell(f);
    fseek(f, 0, SEEK_SET);

    result->data = malloc(result->size);
    fread(result->data, 1, result->size, f);
    fclose(f);
}

void _spPerspectiveMatrixReversedZ(float fovy, float aspect, float near, float far, mat4 dest) {
    glm_mat4_zero(dest);
    float f = 1.0f / tanf(fovy * 0.5f);
    float range = far / (near - far);
    memcpy(dest, &(mat4){
        {f / aspect, 0.0f, 0.0f, 0.0f}, // first COLUMN
        {0.0f, f, 0.0f, 0.0f}, // second COLUMN
        {0.0f, 0.0f, -range - 1, -1.0f}, // third COLUMN
        {0.0f, 0.0f, -near * range, 0.0f} // fourth COLUMN
    }, sizeof(mat4));
}

void _spPerspectiveMatrixReversedZInfiniteFar(float fovy, float aspect, float near, mat4 dest) {
    glm_mat4_zero(dest);
    float f = 1.0f / tanf(fovy * 0.5f);
    memcpy(dest, &(mat4){
        {f / aspect, 0.0f, 0.0f, 0.0f}, // first COLUMN
        {0.0f, f, 0.0f, 0.0f}, // second COLUMN
        {0.0f, 0.0f, 0.0f, -1.0f}, // third COLUMN
        {0.0f, 0.0f, near, 0.0f} // fourth COLUMN
    }, sizeof(mat4));
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
#endif // SPIDER_H_