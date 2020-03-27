#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform UBODynamic {
    mat4 model;
};

layout(set = 0, binding = 1) uniform UBOCommon {
    mat4 view;
    mat4 proj;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoords;

void main() {
    gl_Position = proj * view * model * vec4(inPosition, 1.0);
}