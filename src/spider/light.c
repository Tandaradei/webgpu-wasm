#include "light.h"

#include "debug.h"
#include "impl.h"
#include "state.h"

extern _SPState _sp_state;

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