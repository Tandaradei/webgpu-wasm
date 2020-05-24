#ifndef SPIDER_MATERIAL_H_
#define SPIDER_MATERIAL_H_

#include <webgpu/webgpu.h>
#include <cglm/cglm.h>

typedef struct _SPMaterialTexture {
    WGPUTexture texture;
    WGPUTextureView view;
} _SPMaterialTexture;

typedef struct SPMaterialProperties {
    float roughness;
    float metallic;
    float ao;
} SPMaterialProperties;

typedef struct _SPUboMaterialProperties {
    vec4 rmap; // roughness, metallic, ao, padding
} _SPUboMaterialProperties;

typedef struct SPMaterial {
    struct {
        WGPUBindGroup vert;
        WGPUBindGroup frag;
    } bind_groups;
    SPMaterialProperties props;
    _SPMaterialTexture albedo;
    _SPMaterialTexture normal;
    _SPMaterialTexture ao_roughness_metallic;
    WGPUSampler sampler;
} SPMaterial;

typedef struct SPMaterialDesc {
    const char* albedo;
    const char* normal;
    const char* ao_roughness_metallic;
} SPMaterialDesc;

typedef struct SPMaterialID {
    uint32_t id;
} SPMaterialID;

typedef struct _SPTextureViewFromImageDescriptor {
    _SPMaterialTexture* mat_tex;
    const char* file;
    uint8_t channel_count;
    WGPUTextureViewDescriptor* tex_view_desc;
} _SPTextureViewFromImageDescriptor;

/*
Creates a material and returns an identifier to it
*/
SPMaterialID spCreateMaterial(const SPMaterialDesc* desc);

//
/*
foreach desc in descriptors:
- Loads an image with desc.tex_file_desc info
- Creates a WGPUTexture object and stores it in desc.mat_tex->texture
- Creates a WGPUTextureView object and stores it in desc.mat_tex->view
- Uploads image data to texture via a temporary buffer and copyBufferToTexture
*/
void _spCreateAndLoadTextures(_SPTextureViewFromImageDescriptor descriptors[], const size_t count);

#endif // SPIDER_MATERIAL_H_