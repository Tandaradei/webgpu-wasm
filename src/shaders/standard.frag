#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform MaterialProperties {
    float specular;
} mat;

layout(set = 1, binding = 1) uniform sampler diffuse_sampler;
layout(set = 1, binding = 2) uniform texture2D diffuse_tex;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoords;
layout(location = 4) in vec3 lightPos;

layout(location = 0) out vec4 outColor;


void main() {
    const float ambient = 0.05;
    //const vec3 lightPos = vec3(0.0, 2.0, 0.0);
    const vec3 lightColor = vec3(1.0, 0.8, 0.7);
    const float lightRadius = 20.0;

    const vec3 viewPos = vec3(0.0, 0.0, -10.0);
    const vec3 viewDir = normalize(viewPos - fragPosWorld);

    const vec3 norm = normalize(fragNormal);
    const vec3 lightDir = lightPos - fragPosWorld;
    const float lightStrength = pow(1.0 - min(length(lightDir) / lightRadius, 1.0), 3);
    const vec3 lightDirNorm = normalize(lightDir);
    
    const vec3 reflectDir = reflect(-lightDirNorm, norm);  
    const float spec_factor = pow(max(dot(viewDir, reflectDir), 0.0), 128);
    const float specular = mat.specular * spec_factor;  

    const vec3 surface_color = texture(sampler2D(diffuse_tex, diffuse_sampler), fragTexCoords).rgb;
    const float diffuse = max(dot(norm, lightDir), 0.0);
    const vec3 color = (ambient + lightStrength * lightColor * (diffuse + specular)) * surface_color;
    outColor = vec4(color, 1.0);
}