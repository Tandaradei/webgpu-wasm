#ifndef SPIDER_PIPELINES_H_
#define SPIDER_PIPELINES_H_

#include <webgpu/webgpu.h>

#include "shader.h"


// TODO: move to compute_pipeline.h
typedef struct _SPComputePipeline {
    _SPShaderStage shader;
    WGPUBindGroup bind_group;
    WGPUComputePipeline pipeline;
} _SPComputePipeline;

/* 
TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
When vertex-only render pipelines are available, having no frag stage is valid
*/
#define _SP_VERTEX_ONLY_RENDER_PIPELINE_VALID 0

/*
Because you can only bind up to 4 bind groups simultanously with each up to 16 bindings
we're using the following structure (inspired by sokol_gfx WebGPU implementation):
- Up to 8 uniform blocks combined for both vertex and fragment stages
- Up to 8 textures + samplers for vertex stage
- Up to 8 textures + samplers for fragment stage
(- Up to 8 extra textures + samplers for fragment stage - not implemented yet)
*/
#define _SP_BIND_GROUP_COUNT 3

#define _SP_MAX_PIPELINE_UBS 8

typedef struct _SPRenderPipeline {
    WGPUBindGroupLayout uniform_bind_group_layout;
    _SPShaderStage vert;
    _SPShaderStage frag;
    WGPURenderPipeline pipeline;
} _SPRenderPipeline;

#define _SP_MAX_PIPELINE_VERTEX_BUFFERS 1
#define _SP_MAX_PIPELINE_VERTEX_ATTRIBUTES 8

typedef struct _SPVertexBufferLayoutDesc {
    uint64_t array_stride;
    WGPUInputStepMode step_mode;
    WGPUVertexAttributeDescriptor* attributes[_SP_MAX_PIPELINE_VERTEX_ATTRIBUTES];
} _SPVertexBufferLayoutDesc;

typedef struct _SPRenderPipelineDesc {
    _SPVertexBufferLayoutDesc vertex_buffers[_SP_MAX_PIPELINE_VERTEX_BUFFERS];
    
    //_SPShaderUniformBlockDesc uniform_blocks[_SP_MAX_PIPELINE_UBS];
    uint32_t uniform_block_count;
    _SPShaderStageDesc      vert;
    _SPShaderStageDesc      frag;

    WGPUPrimitiveTopology   primitive_topology;
    WGPUIndexFormat         index_format;

    WGPUDepthStencilStateDescriptor     depth_stencil;
    WGPURasterizationStateDescriptor    rasterizer;
    WGPUColorStateDescriptor            color;
} _SPRenderPipelineDesc;

_SPRenderPipeline _spCreateRenderPipeline(const _SPRenderPipelineDesc* desc);
void _spDestroyRenderPipeline(_SPRenderPipeline render_pipeline);

#endif // SPIDER_PIPELINES_H_