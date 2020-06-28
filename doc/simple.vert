#version 450

layout(set = 0, binding = 0) uniform Common {
    mat4 view;
    mat4 proj;
};

layout(set = 1, binding = 0) uniform Dynamic {
    mat4 model;
};

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec3 outColor;

void main() {
    vec4 pos = proj * view * model * vec4(inPos, 1.0);
    outColor = inColor;
    gl_Position = pos;
}