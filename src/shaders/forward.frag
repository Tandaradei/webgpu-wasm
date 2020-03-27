#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension  GL_EXT_samplerless_texture_functions : enable

layout(set = 1, binding = 0) uniform MaterialProperties {
    float specular;
} mat;

layout(set = 1, binding = 1) uniform sampler my_sampler;
layout(set = 1, binding = 2) uniform texture2D diffuse_tex;
layout(set = 1, binding = 3) uniform texture2D shadow_map;


layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec2 fragTexCoords;
layout(location = 4) in vec4 fragShadowCoord;

layout(location = 0) out vec4 outColor;

int sampleSize = 1;
float scale = 1.5;

float getLightDepthOnPosSingle(vec2 coordsOnShadowMap) {
    float lightDepthOnPos = texture( sampler2D( shadow_map, my_sampler ), coordsOnShadowMap.xy ).r;
    return lightDepthOnPos * 0.5 + 0.5;
}

float getLightDepthOnPosSampled(vec2 coordsOnShadowMap) {
    float value = 0.0;
    ivec2 texDim = textureSize(shadow_map, 0);
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);
    int count = 0;
    for(int sampleY = -sampleSize; sampleY <= sampleSize; ++sampleY) {
        for(int sampleX = -sampleSize; sampleX <= sampleSize; ++sampleX) {
            value += getLightDepthOnPosSingle(coordsOnShadowMap + vec2(sampleX * dx, sampleY * dy));
            count++;
        }
    }
    return value / count;
}


void main() {
    const float ambient = 0.05;
    const vec3 lightColor = vec3(1.0, 0.8, 0.7);
    const float lightRadius = 20.0;
    const vec3 lightPos = vec3(-10.0, 10.0, 0.0);

    vec4 posInLightSpace = fragShadowCoord / fragShadowCoord.w;
    float lightStrength = 1.0;
    float lightDepthOnPos = 0.0;
    posInLightSpace.xyz = posInLightSpace.xyz * 0.5 + 0.5;
    posInLightSpace.y = 1.0 - posInLightSpace.y;
    if ( posInLightSpace.z > 0.0 && posInLightSpace.z < 1.0 ) 
	{
		lightDepthOnPos = getLightDepthOnPosSampled(posInLightSpace.xy);
		if ( posInLightSpace.w > 0.0 && posInLightSpace.z > lightDepthOnPos + 0.001) 
		{
			lightStrength = 0.0;
		}
	}

    const vec3 viewPos = vec3(0.0, 0.0, -10.0);
    const vec3 viewDir = normalize(viewPos - fragPosWorld);
    const vec3 surface_color = texture(sampler2D(diffuse_tex, my_sampler), fragTexCoords).rgb;
    const vec3 norm = normalize(fragNormal);
    const vec3 lightDir = lightPos - fragPosWorld;
    const vec3 lightDirNorm = normalize(lightDir);
    
    const vec3 reflectDir = reflect(-lightDirNorm, norm);  
    const float spec_factor = pow(max(dot(viewDir, reflectDir), 0.0), 128);
    const float specular = mat.specular * spec_factor; 
    const float diffuse = max(dot(norm, lightDirNorm), 0.0);
    const vec3 color = (ambient + lightStrength * lightColor * (diffuse + specular)) * surface_color;
    outColor = vec4(color, 1.0);

    
}