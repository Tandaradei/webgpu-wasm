#ifndef SPIDER_STATE_H_
#define SPIDER_STATE_H_

#include <webgpu/webgpu.h>

#include "camera.h"
#include "mesh.h"
#include "material.h"
#include "instance.h"
#include "pipelines.h"
#include "light.h"

#define _SP_MATERIAL_POOL_MAX 8
#define _SP_MESH_POOL_MAX 256
#define _SP_INSTANCE_POOL_MAX 256
#define _SP_LIGHT_POOL_MAX 8

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

#define _SP_GET_DEFAULT_IF_ZERO(value, default_value) value ? value : default_value


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

void _spErrorCallback(WGPUErrorType type, char const * message, void * userdata);


// Matrices
/* Updates the view matrix */
void _spUpdateView(void);
/* Updates the projection matrix */
void _spUpdateProjection(void);

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


#endif // SPIDER_STATE_H_