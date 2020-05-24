#include "ubos.h"

#include "impl.h"
#include "debug.h"
#include "mesh.h"
#include "material.h"
#include "instance.h"
#include "light.h"
#include "color.h"
#include "state.h"

extern _SPState _sp_state;

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