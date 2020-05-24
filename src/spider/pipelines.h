#ifndef SPIDER_PIPELINES_H_
#define SPIDER_PIPELINES_H_

#include <webgpu/webgpu.h>

typedef struct _SPShaderStage {
    WGPUShaderModule module;
    WGPUBindGroupLayout bind_group_layout;
} _SPShaderStage;

typedef struct _SPRenderPipeline {
    _SPShaderStage vert;
    _SPShaderStage frag;
    WGPUBindGroup bind_group;
    WGPURenderPipeline pipeline;
} _SPRenderPipeline;

typedef struct _SPComputePipeline {
    _SPShaderStage shader;
    WGPUBindGroup bind_group;
    WGPUComputePipeline pipeline;
} _SPComputePipeline;

#endif // SPIDER_PIPELINES_H_