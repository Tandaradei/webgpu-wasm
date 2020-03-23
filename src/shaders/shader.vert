#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UBODynamic {
    mat4 model;
} ubo_dyn;

layout(binding = 1) uniform UBOCommon {
    mat4 view;
    mat4 proj;
} ubo_com;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormal;

void main() {
    vec3 pos_world = vec3(ubo_dyn.model * vec4(inPosition, 1.0));
    gl_Position = ubo_com.proj * ubo_com.view * vec4(pos_world, 1.0);
    fragColor = inColor;
    fragPosWorld = pos_world;
    fragNormal = mat3(transpose(inverse(ubo_dyn.model))) * inNormal;
}