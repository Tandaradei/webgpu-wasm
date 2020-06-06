#version 450

layout(set = 0, binding = 0) uniform Display {
    vec2 size;
} display;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord0;
layout(location = 2) in vec4 color0;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec4 color;

void main() {
    gl_Position = vec4(((position/display.size)-0.5)*vec2(2.0,-2.0), 0.5, 1.0);
    uv = texcoord0;
    color = color0;
}