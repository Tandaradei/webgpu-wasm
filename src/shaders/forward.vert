#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform Model {
    mat4 model;
};

layout(set = 0, binding = 1) uniform Camera {
    mat4 view;
    mat4 proj;
    vec3 pos;
} cam;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoords;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 fragPosWorld;
layout(location = 1) out vec2 fragTexCoords;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragTangent;

void main() {
    vec3 pos_world = vec3(model * vec4(inPosition, 1.0));
    gl_Position = cam.proj * cam.view * vec4(pos_world, 1.0);
    fragPosWorld = pos_world;
    fragTexCoords = inTexCoords;
    mat3 to_world = mat3(transpose(inverse(model)));
    fragNormal = to_world * inNormal;
    fragTangent = to_world * inTangent;
}