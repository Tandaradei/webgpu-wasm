#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension  GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 1) uniform Camera {
    mat4 view;
    mat4 proj;
    vec3 pos;
} cam;

layout(set = 0, binding = 2) uniform Light {
    mat4 view;
    mat4 proj;
    vec4 pos3_range1;
    vec4 color3_type1; // type: 0 directional, 1 spot, 2 point
    vec4 dir3_fov1; // dir: for spot & dir, fov: for spot
    vec4 area2_power1_padding1; // area: for dir
} light;  // TODO: support more than 1 light

layout(set = 2, binding = 0) uniform texture2D albedo_tex;
layout(set = 2, binding = 1) uniform sampler albedo_sampler;
layout(set = 2, binding = 2) uniform texture2D normal_tex;
layout(set = 2, binding = 3) uniform sampler normal_sampler;
layout(set = 2, binding = 4) uniform texture2D ao_roughness_metallic_tex;
layout(set = 2, binding = 5) uniform sampler arm_sampler;
layout(set = 2, binding = 6) uniform texture2D shadow_map;
layout(set = 2, binding = 7) uniform sampler shadow_sampler;


layout(location = 0) in vec3 fragPosWorld;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragTangent;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float gamma = 2.2;

const int sampleSize = 1;
const float scale = 1.5;

float getLightDepthOnPosSingle(vec2 coords_shadow_map) {
    return texture( sampler2D( shadow_map, shadow_sampler ), coords_shadow_map.xy ).r;
}

float getLightDepthOnPosSampled(vec2 coords_shadow_map) {
    float value = 0.0;
    ivec2 texDim = textureSize(shadow_map, 0);
    float dx = scale * 1.0 / float(texDim.x);
    float dy = scale * 1.0 / float(texDim.y);
    int count = 0;
    for(int y = -sampleSize; y <= sampleSize; ++y) {
        for(int x = -sampleSize; x <= sampleSize; ++x) {
            value += getLightDepthOnPosSingle(coords_shadow_map + vec2(x * dx, y * dy));
            count++;
        }
    }
    return value / count;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    const vec4 albedo_all = texture(sampler2D(albedo_tex, albedo_sampler), fragTexCoords).rgba;
    const vec3 albedo = albedo_all.rgb;
    const float alpha = albedo_all.a;
    if(alpha == 0.0) {
        discard;
    }
    vec3 tangent_space_normal = normalize(texture(sampler2D(normal_tex, normal_sampler), fragTexCoords).xyz * vec3(2.0, 2.0, 1.0) - vec3(1.0, 1.0, 0.0));
    const vec3 ao_roughness_metallic = texture(sampler2D(ao_roughness_metallic_tex, arm_sampler), fragTexCoords).rgb;
    const float ao          = ao_roughness_metallic.r;
    const float roughness   = ao_roughness_metallic.g;
    const float metallic    = ao_roughness_metallic.b;

    vec3 Normal = normalize(fragNormal);
    vec3 Tangent = normalize(fragTangent);
    // enforce orthogonality
    Tangent = normalize(Tangent - dot(Tangent, Normal) * Normal);
    const vec3 Bitangent = cross(Tangent, Normal);
    mat3 TBN = mat3(Tangent, Bitangent, Normal);
    vec3 normal = normalize(TBN * tangent_space_normal);

    vec3 N = normalize(normal);
    vec3 V = normalize(cam.pos - fragPosWorld);
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
    vec3 light_pos = light.pos3_range1.xyz;
    float distance    = length(light_pos - fragPosWorld);
    vec3 L = normalize(light_pos - fragPosWorld);
    float light_fov = 1.0 - (light.dir3_fov1.w / 3.14);
    float attenuation = light.area2_power1_padding1.z * (light_fov * light_fov);
    attenuation *= max(1.0 - distance / light.pos3_range1.w, 0.0);
    
    // For spot lights
    if(light.color3_type1.w == 1.0) {
        const float light_angle_rad = acos(dot(light.dir3_fov1.xyz, -L));
        attenuation *= pow(max(light.dir3_fov1.w * 0.5 - light_angle_rad, 0.0) / 3.14, 1.0 - (light_fov * light_fov));
    }
    // Shadow calculation
    const vec4 pos_shadow_map = light.proj * light.view * vec4(fragPosWorld, 1.0);
    vec4 pos_in_light_clip_space = pos_shadow_map / pos_shadow_map.w;
    pos_in_light_clip_space.xy = pos_in_light_clip_space.xy * 0.5 + 0.5; // [-1,1] to [0,1]
    pos_in_light_clip_space.y = 1.0 - pos_in_light_clip_space.y; // bottom-up to top-down
    // [0, 0] of pos_in_light_clip_space.xy should now be the top left corner and [1, 1] the bottom right --> texture space

    const float depth_bias = 0.00001; // Depth bias is not yet implemented in Chromium/dawn, so we have to "fake" it in the shader
    if(attenuation > 0.0 && pos_in_light_clip_space.z > 0.0 && pos_in_light_clip_space.z < 1.0) {
        const float light_depth_on_pos = getLightDepthOnPosSampled(pos_in_light_clip_space.xy);
        if(pos_in_light_clip_space.w > 0.0 && light_depth_on_pos - depth_bias > pos_in_light_clip_space.z) {
            attenuation = 0.0;
        }
    }
    if(attenuation > 0.0) {
        // calculate per-light radiance
        vec3 H = normalize(V + L);
        vec3 radiance     = light.color3_type1.rgb * attenuation;
        
        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular     = numerator / max(denominator, 0.001);  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
  
    vec3 ambient = vec3(0.04) * albedo * ao;
    vec3 color = ambient + Lo;
	
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/gamma));  
   
    outColor = vec4(color, alpha);
}