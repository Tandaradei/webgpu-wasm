#include "state.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include "debug.h"
#include "impl.h"

#include "instance.h"
#include "light.h"
#include "ubos.h"
#include "file.h"

extern _SPState _sp_state;

void spInit(const SPInitDesc* desc) {
    _sp_state.device = emscripten_webgpu_get_device();
    DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: got device\n");
    wgpuDeviceSetUncapturedErrorCallback(_sp_state.device, &_spErrorCallback, NULL);

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

    // Light uniform buffer creation
    const uint32_t light_count = (_sp_state.pools.light_pool.size - 1);
    {
        WGPUBufferDescriptor buffer_desc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = _sp_state.dynamic_alignment * light_count,
        };

        _sp_state.buffers.uniform.light = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }

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

}

void spUpdate(void) {
    _spUpdate();
}


void spRender(void) {
    DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: start\n");

    WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);
    
    _spSortInstances();

    // shadow maps
    // ***
    {
        SPLight* light = &(_sp_state.pools.lights[1]); // TODO: currently just 1 light supported
        
        // TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
        // remove color attachment when vertex-only render pipelines are available
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

        // TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
        // remove color attachment when vertex-only render pipelines are available
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
            wgpuRenderPassEncoderSetBindGroup(shadow_pass_enc, 0, _sp_state.pipelines.render.shadow.bind_group, ARRAY_LEN(offsets_vert), offsets_vert);
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

void _spErrorCallback(WGPUErrorType type, char const * message, void * userdata) {
    printf("[%d] error(%d): %s", _sp_state.frame_index, (int)type, message);
}


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
    pipeline->bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);

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
        .fragmentStage = &frag_stage_desc, // enable "No Color Output" mode
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
// ***