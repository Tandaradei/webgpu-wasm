#ifndef SPIDER_LIGHT_H_
#define SPIDER_LIGHT_H_

#include <webgpu/webgpu.h>
#include <cglm/cglm.h>

#include "color.h"

typedef enum SPLightType {
    SPLightType_Directional = 0,
    SPLightType_Spot = 1,
    SPLightType_Point = 2,
    SPLightType_Force32 = 0x7FFFFFFF,
} SPLightType;

typedef struct SPLight {
    mat4 view;
    mat4 proj;
    vec3 pos;
    float range;
    vec3 dir; // for spot & dir
    float fov; // for spot
    vec2 area; // for dir
    float power;
    SPColorRGB8 color;
    SPLightType type;
    WGPUTexture depth_texture;
    WGPUTextureView depth_view;
    // TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
    // remove color texture + view when vertex-only render pipelines are available
    WGPUTexture color_texture;
    WGPUTextureView color_view;
} SPLight;

typedef struct SPLightShadowCastDesc {
    uint32_t shadow_map_size;
} SPLightShadowCastDesc;

typedef struct SPSpotLightDesc {
    vec3 pos;
    float range;
    SPColorRGB8 color;
    vec3 dir;
    float fov;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPSpotLightDesc;

typedef struct SPDirectionalLightDesc {
    vec3 pos;
    float range;
    vec3 color;
    vec3 dir;
    vec2 area;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPDirectionalLightDesc;

typedef struct SPPointLightDesc {
    vec3 pos;
    float range;
    vec3 color;
    float power;
    const SPLightShadowCastDesc* shadow_casting;
} SPPointLightDesc;

typedef struct SPLightID {
    uint32_t id;
} SPLightID;


/*
Creates a light of type 'spot' and returns an identifier to it
*/
SPLightID spCreateSpotLight(const SPSpotLightDesc* desc);
/*
Creates a light of type 'directional' and returns an identifier to it
*/
SPLightID spCreateDirectionalLight(const SPDirectionalLightDesc* desc);
/*
Creates a light of type 'point' and returns an identifier to it
*/
SPLightID spCreatePointLight(const SPPointLightDesc* desc);

#endif // SPIDER_LIGHT_H_