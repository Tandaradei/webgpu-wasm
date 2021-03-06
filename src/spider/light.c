#include "light.h"

#include "debug.h"
#include "impl.h"
#include "state.h"

extern _SPState _sp_state;

SPLightID spCreateSpotLight(const SPSpotLightDesc* desc){
    DEBUG_PRINT(DEBUG_PRINT_GENERAL, "create light: start\n");
    SPLightID light_id = (SPLightID){_spAllocPoolIndex(&(_sp_state.pools.light.info))};
    if(light_id.id == SP_INVALID_ID) {
        return light_id;
    } 
    SPLight* light = spGetLight(light_id);
    if(!light) {
        return (SPLightID){SP_INVALID_ID};
    }

    light->type = SPLightType_Spot;
    glm_mat4_copy(GLM_MAT4_IDENTITY, light->view);
    glm_mat4_copy(GLM_MAT4_IDENTITY, light->proj);
    glm_vec3_copy((float*)desc->pos, light->pos);
    light->range = desc->range;
    light->color = desc->color;
    glm_vec3_copy((float*)desc->dir, light->dir);
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
            .usage = WGPUTextureUsage_OutputAttachment | WGPUTextureUsage_Sampled, // TODO: should sample directly from depth texture
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

        // TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
        // remove color texture when vertex-only render pipelines are available
        WGPUTextureDescriptor color_tex_desc = {
            .usage = WGPUTextureUsage_OutputAttachment,
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