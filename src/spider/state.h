#ifndef SPIDER_STATE_H_
#define SPIDER_STATE_H_

#include <webgpu/webgpu.h>

#include "camera.h"
#include "mesh.h"
#include "material.h"
#include "scene_node.h"
#include "render_pipeline.h"
#include "light.h"
#include "imgui_impl_spider.h"
#include "input.h"

#define _SP_MATERIAL_POOL_DEFAULT 8
#define _SP_MESH_POOL_DEFAULT 256
#define _SP_RENDER_MESH_POOL_DEFAULT 512
#define _SP_LIGHT_POOL_DEFAULT 8
#define _SP_SCENE_NODE_POOL_DEFAULT 1024

#define _SP_SURFACE_WIDTH_DEFAULT 1280
#define _SP_SURFACE_HEIGHT_DEFAULT 720

typedef struct _SPPool {
    size_t size;
    size_t last_index_plus_1;
    size_t queue_top;
    uint32_t* gen_ctrs;
    uint32_t* free_queue;
} _SPPool;

typedef struct SPPoolsDesc {
    const struct {
        uint32_t materials;
        uint32_t meshes;
        uint32_t render_meshes;
        uint32_t lights;
        uint32_t scene_nodes;
    } capacities;
} SPPoolsDesc;

typedef struct SPInitDesc {
    const struct {
        size_t width;
        size_t height;
    } surface_size;

    bool (*update_func)(float delta_time_s);
    uint32_t fps_limit;

    const SPCamera camera;

    SPPoolsDesc pools;

    bool show_stats;
} SPInitDesc;

typedef struct _SPPools {
    struct {
        _SPPool info;
        SPMaterial* data;
    } material;

    struct {
        _SPPool info;
        SPMesh* data;
    } mesh;

    struct {
        _SPPool info;
        SPRenderMesh* data;
    } render_mesh;

    struct {
        _SPPool info;
        SPLight* data;
    } light;

    struct {
        _SPPool info;
        SPSceneNode* data;
    } scene_node;
} _SPPools;

typedef struct _SPBuffers {
    struct {
        WGPUBuffer camera;
        WGPUBuffer camera_staging;
        WGPUBuffer model;
        WGPUBuffer model_staging;
        WGPUBuffer light;
        WGPUBuffer light_staging;
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

    clock_t start_clock;
    bool (*update_func)(float delta_time_s);
    uint32_t fps_limit;

    uint32_t dynamic_alignment;

    uint32_t frame_index; // enough for ~10,000 hours @ 120 fps

    struct {
        struct {
            _SPRenderPipeline forward;
            _SPRenderPipeline shadow;
        } render;
        struct {
            _SPComputePipeline mipmaps;
        } compute;
    } pipelines;

    uint32_t* rm_counts_per_mat;
    SPRenderMeshID* sorted_rm;

    struct {
        SPSceneNode** data;
        uint32_t count;
    } dirty_nodes;

    struct {
        _SPMaterialTexture normal;
        _SPMaterialTexture ao_roughness_metallic;
        _SPMaterialTexture ao;
    } default_textures;

    _SPImGuiState imgui;
    _SPInputState input;
    
    bool show_stats;

    WGPUBindGroup shadow_bind_group;
    char test_buffer[100];

} _SPState;

/* 
Initializes the application and creates all static resources
*/
void spInit(const SPInitDesc* desc);

/*
Releases all remaining resources and frees allocated data
*/
void spShutdown(void);

/*
Starts the application update cycle
*/
void spStart(void);

/*
Returns a temporary pointer to the active camera 
*/
SPCamera* spGetActiveCamera();
/*
Returns a temporary pointer to the scene node with the specified id
NULL if not a valid id
*/
SPMaterial* spGetMaterial(SPMaterialID mat_id);
/*
Returns a temporary pointer to the scene node with the specified id
NULL if not a valid id
*/
SPMesh* spGetMesh(SPMeshID mesh_id);
/*
Returns a temporary pointer to the scene node with the specified id
NULL if not a valid id
*/
SPSceneNode* spGetSceneNode(SPSceneNodeID node_id);
/*
Returns a temporary pointer to the render mesh with the specified id
NULL if not a valid id
*/
SPRenderMesh* spGetRenderMesh(SPRenderMeshID rm_id);
/*
Returns a temporary pointer to the light with the specified id
NULL if not a valid id
*/
SPLight* spGetLight(SPLightID light_id);

SPKeyState spGetKeyState(SPKey key);
bool spGetKeyPressed(SPKey key);
bool spGetKeyDown(SPKey key);
bool spGetKeyUp(SPKey key);
SPMouseButtonState spGetMouseButtonState(SPMouseButton button);
bool spGetMouseButtonPressed(SPMouseButton button);
bool spGetMouseButtonDown(SPMouseButton button);
bool spGetMouseButtonUp(SPMouseButton button);
uint32_t spGetMousePositionX();
uint32_t spGetMousePositionY();

// ----------------------------------
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
TODO: Implement
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
uint32_t _spAllocPoolIndex(_SPPool* pool);
/* Free an ID from a pool */
void _spFreePoolIndex(_SPPool* pool, int slot_index);

// TODO: Add description
void _spErrorCallback(WGPUErrorType type, char const * message, void * userdata);

typedef struct EmscriptenKeyboardEvent EmscriptenKeyboardEvent;
int _spEmscriptenKeyCallback(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData);
typedef struct EmscriptenMouseEvent EmscriptenMouseEvent;
int _spEmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData);

// TODO: Add description
void _spUpdateDirtyNodes(void);

// Matrices
/* Updates the view matrix */
void _spUpdateView(void);
/* Updates the projection matrix */
void _spUpdateProjection(void);

// General
/* 
Updates the model, view and projection matrices and copies them
in their respective buffers buffer
Calls the custom update_func
*/
void _spUpdate(void);
/*
Records the commands for the GPU and submits them
Draws all valid RenderMeshes
*/
void _spRender(void);
/*
Creates a command buffer from the recorded commands
and submits it to the queue
Recreates the command encoder
*/
void _spSubmit(void);

/*
Creates a list for each Material with all the RenderMeshes using the Material
*/
void _spSortRenderMeshes(void);

// TODO: Add description
bool _spIsIDValid(uint32_t id, const _SPPool* pool);

#endif // SPIDER_STATE_H_