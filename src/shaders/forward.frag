#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension  GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 1) uniform Camera {
    mat4 view;
    mat4 proj;
    vec3 pos;
} cam;

layout(set = 1, binding = 0) uniform MaterialProperties {
    float ambient;
    float specular;
} mat;

layout(set = 1, binding = 1) uniform Light {
    mat4 view;
    mat4 proj;
    vec4 pos3_range1;
    vec4 color3_type1; // type: 0 directional, 1 spot, 2 point
    vec4 dir3_fov1; // dir: for spot & dir, fov: for spot
    vec4 area2_power1_padding1; // area: for dir
} light;  // TODO: support more than 1 light

layout(set = 1, binding = 2) uniform sampler my_sampler;
layout(set = 1, binding = 3) uniform texture2D albedo_tex;
layout(set = 1, binding = 4) uniform texture2D shadow_map; // TODO: support more than 1 shadow map


layout(location = 0) in vec3 fragPosWorld;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragTangent;

layout(location = 0) out vec4 outColor;

int sampleSize = 1;
float scale = 1.5;

float getLightDepthOnPosSingle(vec2 coords_shadow_map) {
    float light_depth_on_pos = texture( sampler2D( shadow_map, my_sampler ), coords_shadow_map.xy ).r;
    return light_depth_on_pos * 0.5 + 0.5; // Convert [-1, 1] to [0, 1]
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


void main() {
    const vec3 albedo_color = texture(sampler2D(albedo_tex, my_sampler), fragTexCoords).rgb;
    vec3 color = mat.ambient * albedo_color;

    vec3 light_pos = light.pos3_range1.xyz;
    vec3 light_dir = light.dir3_fov1.xyz;

    const vec3 pos_to_light = light_pos - fragPosWorld;
    const vec3 pos_to_light_norm = normalize(pos_to_light);
    float light_fov = 1.0 - (light.dir3_fov1.w / 3.14);
    float light_power = light.area2_power1_padding1.z * (light_fov * light_fov);
    light_power *= max(1.0 - length(pos_to_light) / light.pos3_range1.w, 0.0);
    
    // For spot lights
    if(light.color3_type1.w == 1.0) {
        const float light_angle_rad = acos(dot(light_dir, -pos_to_light_norm));
        light_power *= pow(max(light.dir3_fov1.w * 0.5 - light_angle_rad, 0.0) / 3.14, 1.0 - (light_fov * light_fov));
    }
    // If still in light area
    if(light_power > 0.0) {
        // Shadow calculation
        const vec4 pos_shadow_map = light.proj * light.view * vec4(fragPosWorld, 1.0);
        vec4 pos_in_light_clip_space = pos_shadow_map / pos_shadow_map.w;
        pos_in_light_clip_space.xyz = pos_in_light_clip_space.xyz * 0.5 + 0.5; // [-1,1] to [0,1]
        pos_in_light_clip_space.y = 1.0 - pos_in_light_clip_space.y; // bottom-up to top-down
        if ( pos_in_light_clip_space.z > 0.0 && pos_in_light_clip_space.z < 1.0 ) 
        {
            const float light_depth_on_pos = getLightDepthOnPosSampled(pos_in_light_clip_space.xy);
            if ( pos_in_light_clip_space.w > 0.0 && pos_in_light_clip_space.z > light_depth_on_pos + 0.001) 
            {
                light_power = 0.0;
            }
        }
    }
    // If not in shadow
    if(light_power > 0.0) {
        const vec3 norm = normalize(fragNormal);
        // Calculate diffuse factor
        const float diffuse = max(dot(norm, pos_to_light_norm), 0.0);
        // Calculate specular factor
        const vec3 view_dir_norm = normalize(cam.pos - fragPosWorld);
        const vec3 reflect_dir = reflect(-pos_to_light_norm, norm);  
        const float spec_factor = pow(max(dot(view_dir_norm, reflect_dir), 0.0), 32);
        const float specular = mat.specular * spec_factor; 
        // Calculate pixel color
        color = (mat.ambient + light_power * light.color3_type1.rgb * (diffuse + specular)) * albedo_color;
    }

    vec3 ldrColor = color;
    if(gl_FragCoord.x >= 960) {
        // reinhard tone mapping
        vec3 mapped = color / (color + vec3(1.0));
        // gamma correction 
        const float gamma = 2.2;
        ldrColor = pow(mapped, vec3(1.0 / gamma));
    }

    outColor = vec4(ldrColor, 1.0);
}