#version 450

layout(location = 0) out float outColor;

void main() {
    outColor = gl_FragCoord.z;
}