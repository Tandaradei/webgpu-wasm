#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;


void main() {
    const float ambient = 0.1;
    const float specularStrength = 0.5;
    const vec3 lightPos = vec3(0.0, 2.0, 0.0);
    const vec3 lightColor = vec3(1.0, 1.0, 1.0);

    const vec3 viewPos = vec3(0.0, 0.0, -10.0);
    const vec3 viewDir = normalize(viewPos - fragPosWorld);

    const vec3 norm = normalize(fragNormal);
    const vec3 lightDir = normalize(lightPos - fragPosWorld);
    
    const vec3 reflectDir = reflect(-lightDir, norm);  
    const float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    const vec3 specular = specularStrength * spec * lightColor;  

    const float diff = max(dot(norm, lightDir), 0.0);
    const vec3 diffuse = diff * lightColor;
    const vec3 color = (diffuse + ambient + specular) * fragColor;
    outColor = vec4(color, 1.0);
}