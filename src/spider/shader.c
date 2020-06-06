#include "shader.h"

#include "file.h"
#include "debug.h"
#include "impl.h"

#include "state.h"
extern _SPState _sp_state;

WGPUShaderModule _spCreateShaderModuleFromSpirVFile(const char* filename) {
    _SPFileReadResult result;
    _spReadFile(filename, &result);
    DEBUG_PRINT(DEBUG_PRINT_SHADER_LOAD, "read file: size: %d\n", result.size);
    WGPUShaderModule shader_module = _spCreateShaderModuleFromSpirVBytecode(result.data, result.size);
    free(result.data);
    return shader_module;
}

WGPUShaderModule _spCreateShaderModuleFromSpirVBytecode(const uint8_t* data, const uint32_t size) {
    SP_ASSERT(data && size > 0 && size % 4 == 0);
    WGPUShaderModuleSPIRVDescriptor shader_module_desc = {
        .chain = {
            .sType = WGPUSType_ShaderModuleSPIRVDescriptor,
        },
        .codeSize = size / sizeof(uint32_t),
        .code = (const uint32_t*)data
    };
    WGPUShaderModuleDescriptor shader_module_wrapper_desc = {
        .nextInChain = (WGPUChainedStruct const*)&shader_module_desc,
    };
    return wgpuDeviceCreateShaderModule(_sp_state.device, &shader_module_wrapper_desc);
}

_SPShaderStage _spCreateShaderStage(const _SPShaderStageDesc* desc, WGPUShaderStage visibility) {
    SP_ASSERT(desc->file || desc->byte_code.data && desc->byte_code.size > 0);

    WGPUShaderModule shader_module;
    if(desc->file) {
        shader_module = _spCreateShaderModuleFromSpirVFile(desc->file);
    } 
    else {
        shader_module = _spCreateShaderModuleFromSpirVBytecode(desc->byte_code.data, desc->byte_code.size);
    }
    SP_ASSERT(shader_module);

    // We need a sampler for each texture, therefore double the entries
    WGPUBindGroupLayoutEntry entries[_SP_MAX_SHADERSTAGE_TEXTURES * 2];
    uint32_t entry_count = 0;
    for(uint32_t t = 0; t < _SP_MAX_SHADERSTAGE_TEXTURES; t++) {
        if(desc->textures[t].view_dimension != WGPUTextureViewDimension_Undefined) {
            entries[entry_count++] = (WGPUBindGroupLayoutEntry){
                .binding = t*2,
                .visibility = visibility,
                .type = WGPUBindingType_SampledTexture,
                .viewDimension = desc->textures[t].view_dimension,
                .multisampled = false,
                .textureComponentType = desc->textures[t].texture_component_type,
            };
            entries[entry_count++] = (WGPUBindGroupLayoutEntry){
                .binding = t*2+1,
                .visibility = visibility,
                .type = WGPUBindingType_Sampler,
                .viewDimension = desc->textures[t].view_dimension,
                .multisampled = false,
                .textureComponentType = desc->textures[t].texture_component_type,
            };
        }
    }
    for(uint32_t i = 0; i < entry_count; i++) {
        DEBUG_PRINT(DEBUG_PRINT_RENDER_PIPELINE, "%s: binding: %u\n", visibility & WGPUShaderStage_Vertex ? "vert" : "frag", entries[i].binding);
    }

    WGPUBindGroupLayoutDescriptor bgl_desc = {
        .entryCount = entry_count,
        .entries = entries
    };

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &bgl_desc);
    SP_ASSERT(bgl);

    return (_SPShaderStage) {
        .module = shader_module,
        .bind_group_layout = bgl,
        .entry = desc->entry ? desc->entry : "main",
    };
}