#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBODynamic {
    mat4 model;
};

layout(set = 0, binding = 1) uniform UBOCommon {
    mat4 view;
    mat4 proj;
};

layout(set = 0, binding = 2) uniform UBOCommonLight {
    mat4 view;
    mat4 proj;
} light;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec2 fragTexCoords;
layout(location = 4) out vec4 fragShadowCoord;

void main() {
    vec3 pos_world = vec3(model * vec4(inPosition, 1.0));
    gl_Position = proj * view * vec4(pos_world, 1.0);
    fragColor = inColor;
    fragPosWorld = pos_world;
    fragNormal = mat3(transpose(inverse(model))) * inNormal;
    fragTexCoords = inTexCoords;
    vec4 coordsOnShadowMap = light.proj * light.view * vec4(pos_world, 1.0);
    float ndcDepth =
        (2.0 * coordsOnShadowMap.z - 0.1 - 100.0) /
        (100.0 - 0.1);
    float clipDepth = ndcDepth / coordsOnShadowMap.w;
    fragShadowCoord = vec4(coordsOnShadowMap.xy, coordsOnShadowMap.z, coordsOnShadowMap.w);
}