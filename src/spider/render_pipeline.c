#include "render_pipeline.h"

#include "impl.h"
#include "debug.h"
#include "state.h"

extern _SPState _sp_state;

_SPRenderPipeline _spCreateRenderPipeline(const _SPRenderPipelineDesc* desc) {
    _SPShaderStage vert_stage = _spCreateShaderStage(&desc->vert, WGPUShaderStage_Vertex);
    #if _SP_VERTEX_ONLY_RENDER_PIPELINE_VALID
    WGPUProgrammableStageDescriptor frag_stage_desc;
    bool has_frag_stage = (desc->frag.file || desc->frag.byte_code.data);
    if(has_frag_stage) {
        _SPShaderStage frag_stage = _spCreateShaderStage(&desc->frag, WGPUShaderStage_Fragment);
        frag_stage_desc.module = frag_stage.module;
        frag_stage_desc.entryPoint = frag_stage.entry;
    }
    #else
    bool has_frag_stage = true;
    _SPShaderStage frag_stage = _spCreateShaderStage(&desc->frag, WGPUShaderStage_Fragment);
    WGPUProgrammableStageDescriptor frag_stage_desc = {
        .module = frag_stage.module,
        .entryPoint = frag_stage.entry
    };
    #endif

    WGPUVertexBufferLayoutDescriptor vert_buffer_layout_descs[_SP_MAX_PIPELINE_VERTEX_BUFFERS];
    WGPUVertexAttributeDescriptor vert_attribs[_SP_MAX_PIPELINE_VERTEX_BUFFERS][_SP_MAX_PIPELINE_VERTEX_ATTRIBUTES];
    uint32_t vertex_buffer_count = 0;
    for(uint32_t b = 0; b < _SP_MAX_PIPELINE_VERTEX_BUFFERS; b++) {
        const _SPVertexBufferLayoutDesc* buffer = &desc->vertex_buffers[b];
        if(buffer->array_stride > 0) {
            vert_buffer_layout_descs[vertex_buffer_count].arrayStride = buffer->array_stride;
            vert_buffer_layout_descs[vertex_buffer_count].stepMode = buffer->step_mode;
            uint32_t vert_attrib_count = 0;
            for(uint32_t a = 0; a < _SP_MAX_PIPELINE_VERTEX_ATTRIBUTES; a++) {
                if(buffer->attributes[a]){
                    vert_attribs[b][a] = *buffer->attributes[a];
                    vert_attrib_count++;
                }
            }
            vert_buffer_layout_descs[vertex_buffer_count].attributeCount = vert_attrib_count;
            vert_buffer_layout_descs[vertex_buffer_count].attributes = &vert_attribs[b][0];
            vertex_buffer_count++;
        }
    }

    WGPUVertexStateDescriptor vert_state_desc = {
        .indexFormat = desc->index_format,
        .vertexBufferCount = vertex_buffer_count,
        .vertexBuffers = vert_buffer_layout_descs
    };

    WGPUBindGroupLayoutEntry bgl_uniform_entries[_SP_MAX_PIPELINE_UBS];
    uint32_t bgl_uniform_entry_count = 0;
    for(uint32_t u = 0; u < desc->uniform_block_count; u++) {
        WGPUBindGroupLayoutEntry* entry = &bgl_uniform_entries[bgl_uniform_entry_count++];
        entry->binding = u;
        entry->type = WGPUBindingType_UniformBuffer;
        entry->visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        entry->hasDynamicOffset = true;
        entry->multisampled = false;
    }
    WGPUBindGroupLayoutDescriptor bgl_uniform_desc = {
        .entryCount = bgl_uniform_entry_count,
        .entries = bgl_uniform_entries,
    };
    for(uint32_t i = 0; i < bgl_uniform_entry_count; i++) {
        DEBUG_PRINT(DEBUG_PRINT_RENDER_PIPELINE, "uniform: binding: %u\n", bgl_uniform_entries[i].binding);
    }
    WGPUBindGroupLayout uniform_bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &bgl_uniform_desc);
    SP_ASSERT(uniform_bind_group_layout);

    WGPUBindGroupLayout bgls[_SP_BIND_GROUP_COUNT] = {
        uniform_bind_group_layout,
        vert_stage.bind_group_layout,
        frag_stage.bind_group_layout,
    };
    for(uint32_t i = 0; i < _SP_BIND_GROUP_COUNT; i++) {
        SP_ASSERT(bgls[i]);
    }

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
        .bindGroupLayoutCount = _SP_BIND_GROUP_COUNT,
        .bindGroupLayouts = bgls
    };

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(_sp_state.device, &pipeline_layout_desc);
    SP_ASSERT(pipeline_layout);

    WGPURenderPipelineDescriptor pipeline_desc = {
        .layout = pipeline_layout,
        .vertexStage = {
            .module = vert_stage.module,
            .entryPoint = vert_stage.entry,
        },
        .vertexState = &vert_state_desc,
        .fragmentStage = has_frag_stage ? &frag_stage_desc : NULL,
        .rasterizationState = &desc->rasterizer,
        .sampleCount = 1,
        .depthStencilState = &desc->depth_stencil,
        .colorStateCount = 1,
        .colorStates = &desc->color,
        .primitiveTopology = desc->primitive_topology,
        .sampleMask = 0xFFFFFFFF
    };

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
    SP_ASSERT(pipeline);

    return (_SPRenderPipeline){
        .uniform_bind_group_layout = uniform_bind_group_layout,
        .vert = vert_stage,
        .frag = frag_stage,
        .pipeline = pipeline,
    };
}


void _spDestroyRenderPipeline(_SPRenderPipeline render_pipeline) {
    
}