#include "ubos.h"

#include "impl.h"
#include "debug.h"
#include "mesh.h"
#include "material.h"
#include "light.h"
#include "color.h"
#include "state.h"

extern _SPState _sp_state;

void _spUpdateUboModel(void) {
    if(_sp_state.buffers.uniform.model_staging) {
        wgpuBufferDestroy(_sp_state.buffers.uniform.model_staging);
        wgpuBufferRelease(_sp_state.buffers.uniform.model_staging);
    }
    WGPUBufferDescriptor buffer_desc = {
        .usage = WGPUBufferUsage_CopySrc,
        .size = (_sp_state.pools.render_mesh.info.size - 1) * _sp_state.dynamic_alignment,
    };
    WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
    _sp_state.buffers.uniform.model_staging = result.buffer;

    for(SPSceneNodeID node_id = {1}; node_id.id < _sp_state.pools.scene_node.info.last_index_plus_1; node_id.id++) {
        SPSceneNode* node = spGetSceneNode(node_id);
        if(node && node->linked_object.type == SPSceneNodeType_RenderMesh) {
            SPRenderMeshID rm_id = node->linked_object.render_mesh;
            SPRenderMesh* rm = spGetRenderMesh(rm_id);
            if(rm) {
                _SPUboModel ubo;
                glm_mat4_copy(node->_transform_world, ubo.model);

                uint64_t offset = (rm_id.id - 1) * _sp_state.dynamic_alignment;

                memcpy((void*)((uint64_t)(result.data) + offset), &ubo, sizeof(_SPUboModel));
            }
        }
    }
    wgpuBufferUnmap(result.buffer);
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc,
        result.buffer, 0,
        _sp_state.buffers.uniform.model, 0,
        result.dataLength
    );
}

void _spUpdateUboCamera(void) {
    if(_sp_state.buffers.uniform.camera_staging) {
        wgpuBufferDestroy(_sp_state.buffers.uniform.camera_staging);
        wgpuBufferRelease(_sp_state.buffers.uniform.camera_staging);
    }
    WGPUBufferDescriptor buffer_desc = {
        .usage = WGPUBufferUsage_CopySrc,
        .size = sizeof (_SPUboCamera),
    };
    WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
    _sp_state.buffers.uniform.camera_staging = result.buffer;
    
    _SPUboCamera ubo = {
        .view = GLM_MAT4_IDENTITY_INIT,
        .proj = GLM_MAT4_IDENTITY_INIT,
        .pos = {0.0f, 0.0f, 0.0f}
    };
    memcpy(ubo.view, _sp_state.active_cam._view, sizeof(mat4));
    memcpy(ubo.proj, _sp_state.active_cam._proj, sizeof(mat4));
    memcpy(ubo.pos, _sp_state.active_cam.pos, sizeof(vec3));

    memcpy(result.data, &ubo, sizeof(_SPUboCamera));

    wgpuBufferUnmap(result.buffer);
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc,
        result.buffer, 0,
        _sp_state.buffers.uniform.camera, 0,
        result.dataLength
    );
}

void _spUpdateUboLight(void) {
     if(_sp_state.buffers.uniform.light_staging) {
        wgpuBufferDestroy(_sp_state.buffers.uniform.light_staging);
        wgpuBufferRelease(_sp_state.buffers.uniform.light_staging);
    }
    WGPUBufferDescriptor buffer_desc = {
        .usage = WGPUBufferUsage_CopySrc,
        .size = (_sp_state.pools.light.info.size - 1) * _sp_state.dynamic_alignment,
    };
    WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
    _sp_state.buffers.uniform.light_staging = result.buffer;
    
    //for(uint32_t i = 1; i < _sp_state.pools.light_pool.last_index_plus_1; i++) {
        uint32_t i = 1; // only 1 light supported right now
        SPLight* light = &(_sp_state.pools.light.data[i]);
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

        memcpy((void*)((uint64_t)(result.data) + offset), &(ubo), sizeof(_SPUboLight));
    //}
    wgpuBufferUnmap(result.buffer);
    wgpuCommandEncoderCopyBufferToBuffer(
        _sp_state.cmd_enc,
        result.buffer, 0,
        _sp_state.buffers.uniform.light, 0,
        result.dataLength
    );
}