#version 450

layout(set = 2, binding = 0) uniform texture2D tex0;
layout(set = 2, binding = 1) uniform sampler sampler0;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture( sampler2D( tex0, sampler0 ), uv) * color;
}