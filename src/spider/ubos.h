#ifndef SPIDER_UBOS_H_
#define SPIDER_UBOS_H_

#include <webgpu/webgpu.h>
#include <cglm/cglm.h>

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


typedef struct _SPStagingBufferPool _SPStagingBufferPool;

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

#endif // SPIDER_UBOS_H_