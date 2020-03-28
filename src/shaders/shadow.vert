#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform Model {
    mat4 model;
};

layout(set = 0, binding = 1) uniform Light {
    mat4 view;
    mat4 proj;
    vec4 pos3_range1;
    vec4 color3_type1; // type: 0 directional, 1 spot, 2 point
    vec4 dir3_fov1; // dir: for spot & dir, fov: for spot
    vec4 area2_power1_padding1; // area: for dir
} light;  // TODO: support more than 1 light

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

void main() {
    gl_Position = light.proj * light.view * model * vec4(inPosition, 1.0);
}