#version 450

layout(location = 0) out float outColor;

void main() {
    float ndcDepth =
        (2.0 * gl_FragCoord.z - 0.1 - 100.0) /
        (100.0 - 0.1);
    float clipDepth = ndcDepth / gl_FragCoord.w;
    outColor = gl_FragCoord.z;
}