#include "material.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "debug.h"
#include "impl.h"
#include "file.h"
#include "ubos.h"
#include "state.h"

extern _SPState _sp_state;

SPMaterialID spCreateMaterial(const SPMaterialDesc* desc) {
    SPMaterialID material_id = (SPMaterialID){_spAllocPoolIndex(&(_sp_state.pools.material.info))};
    if(material_id.id == SP_INVALID_ID) {
        return material_id;
    }
    uint32_t id = material_id.id; 
    SPMaterial* material = &(_sp_state.pools.material.data[id]);

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

    _spCreateAndLoadTextures(image_descs, SP_ARRAY_LEN(image_descs));

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
    
    WGPUBindGroupEntry uniform_bindings[] = {
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
        },
        {
            .binding = 2,
            .buffer = _sp_state.buffers.uniform.light,
            .offset = 0,
            .size = sizeof(_SPUboCamera),
            .sampler = NULL,
            .textureView = NULL,
        }
    };

    WGPUBindGroupDescriptor uniform_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.uniform_bind_group_layout,
        .entryCount = SP_ARRAY_LEN(uniform_bindings),
        .entries = uniform_bindings
    };
    material->bind_groups.uniform = wgpuDeviceCreateBindGroup(_sp_state.device, &uniform_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created uniform bind group\n");

    WGPUBindGroupEntry vert_bindings[] = {};
    WGPUBindGroupDescriptor vert_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.vert.bind_group_layout,
        .entryCount = SP_ARRAY_LEN(vert_bindings),
        .entries = vert_bindings
    };
    material->bind_groups.vert = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created vert bind group\n");


    SP_ASSERT(_sp_state.pools.light.info.size > 0 && _sp_state.pools.light.data[1].depth_view);
    
    WGPUBindGroupEntry frag_bindings[] = {
        {
            .binding = 0,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = material->albedo.view,
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
            .textureView = material->normal.view ? material->normal.view :_sp_state.default_textures.normal.view,
        },
        {
            .binding = 3,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = material->sampler,
            .textureView = NULL,
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
            .sampler = material->sampler,
            .textureView = NULL,
        },
        {
            .binding = 6,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = NULL,
            .textureView = _sp_state.pools.light.data[1].depth_view, // TODO: currently just 1 light supported
        },
        {
            .binding = 7,
            .buffer = NULL,
            .offset = 0,
            .size = 0,
            .sampler = material->sampler,
            .textureView = NULL,
        },
    };
    WGPUBindGroupDescriptor frag_bg_desc = {
        .layout = _sp_state.pipelines.render.forward.frag.bind_group_layout,
        .entryCount = SP_ARRAY_LEN(frag_bindings),
        .entries = frag_bindings
    };
    material->bind_groups.frag = wgpuDeviceCreateBindGroup(_sp_state.device, &frag_bg_desc);
    DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "mat_creation: created frag bind group\n");

    return material_id;
}


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
        const uint8_t comp_map[5] = {
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
        SP_ASSERT(pixel_data);
        uint8_t comps = comp_map[read_comps];
        DEBUG_PRINT(DEBUG_PRINT_WARNING, "loaded image %s (%d, %d, %d / %d)\n", file, width, height, read_comps, comps);

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
        SP_ASSERT(result.data && result.dataLength == width * height * comps);
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