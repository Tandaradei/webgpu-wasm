#ifndef SPIDER_SHADER_H_
#define SPIDER_SHADER_H_

#include <webgpu/webgpu.h>

#define _SP_MAX_SHADERSTAGE_TEXTURES 8

typedef struct _SPShaderStage {
    WGPUShaderModule module;
    WGPUBindGroupLayout bind_group_layout;
    const char* entry;
} _SPShaderStage;

typedef struct _SPShaderUniformBlockDesc {
    uint32_t size;
} _SPShaderUniformBlockDesc;

typedef struct _SPShaderTextureDesc {
    WGPUTextureViewDimension view_dimension;
    WGPUTextureComponentType texture_component_type;
} _SPShaderTextureDesc;

typedef struct _SPShaderStageDesc {
    // When both, the file and the byte_code struct have valid data, the file will be used
    const char* file;
    struct {
        const uint8_t* data;
        const uint32_t size;
    } byte_code;

    const char* entry;
    _SPShaderTextureDesc textures[_SP_MAX_SHADERSTAGE_TEXTURES];
} _SPShaderStageDesc;

WGPUShaderModule _spCreateShaderModuleFromSpirVFile(const char* filename);
WGPUShaderModule _spCreateShaderModuleFromSpirVBytecode(const uint8_t* data, const uint32_t size);
_SPShaderStage _spCreateShaderStage(const _SPShaderStageDesc* desc, WGPUShaderStage visibility);
#endif