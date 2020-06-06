#include <emscripten/emscripten.h>
#include "spider/spider.h"

//#include "mesh_data.h" // not used currently

SPLightID spot_light_id;
clock_t start_clock;

float randFloat(void) {
    return (float) rand() / (float) RAND_MAX;
}

float randFloatRange(float min, float max) {
    return min + randFloat() * (max - min);
}

void createObjects(void) {
    // TODO: lights have to be created before materials right now 
    const vec3 light_pos = {0.0f, 5.0f, 0.5f};
    const vec3 light_look_at = {2.0f, 0.0f, 0.0f};
    vec3 light_direction = {-1.0, -1.0f, 0.2f};
    // (float*) cast to prevent compiler warning 'incompatible-pointer-types-discards-qualifiers'
    // cglm takes no const pointers as arguments, even if it doesn't mutate the vectors
    glm_vec3_sub((float*)light_look_at, (float*)light_pos, light_direction);
    glm_vec3_normalize(light_direction);

    spot_light_id = spCreateSpotLight(&(SPSpotLightDesc){
            .pos = {light_pos[0], light_pos[1], light_pos[2]},
            .range = 40.0f,
            .color = {.r = 255, .g = 255, .b = 255},
            .dir = {light_direction[0], light_direction[1], light_direction[2]},
            .fov = glm_rad(70.0f),
            .power = 20.0f,
            .shadow_casting = &(SPLightShadowCastDesc){
                .shadow_map_size = 2048,
            },
        }
    );
    SP_ASSERT(spot_light_id.id != SP_INVALID_ID);

    SPSceneNodeID sponza_node_id = spLoadGltf("assets/gltf/Sponza/Sponza.gltf");
}

static float time_elapsed_total_s = 0.0f;

void frame(void) {
    clock_t cur_clock = clock();
    float delta_time_s = ((float)(cur_clock - start_clock) / CLOCKS_PER_SEC);
    start_clock = clock();
    time_elapsed_total_s += delta_time_s;

    // Update camera
    bool should_animate_cam = true;
    if(should_animate_cam) {
        SPCamera* cam = spGetActiveCamera();
        float rounds_per_second = 0.1f;
        float rotation_speed = rounds_per_second * M_PI;
        cam->dir[0] = sin(time_elapsed_total_s * rotation_speed);
        cam->dir[1] = sin(0.45 + time_elapsed_total_s * 0.63f) * 0.1f;
        cam->dir[2] = cos(time_elapsed_total_s * rotation_speed);
        glm_vec3_normalize(cam->dir);
    }
    
    // Update light(s)
    bool should_animate_light = true;
    if(should_animate_light) {
        SPLight* spot_light = spGetLight(spot_light_id);
        if(spot_light) {
            spot_light->pos[0] = sin(time_elapsed_total_s * 0.05f) * 10.0f;
            vec3 light_look_at = {0.0f, 0.0f, 0.0f};
            vec3 light_direction = {-1.0, -1.0f, 0.2f};
            glm_vec3_sub(light_look_at, spot_light->pos, light_direction);
            glm_vec3_normalize(light_direction);
            memcpy(spot_light->dir, light_direction, sizeof spot_light->dir);
        }
    }

    spUpdate(delta_time_s);
    spRender();
}

int main() {
    srand(0); // to ensure getting comparable results every run
    const uint16_t surface_width = 1280;
    const uint16_t surface_height = 720;
    vec3 dir = {0.0f, 0.0f, -1.0f}; // for SPCameraMode_Direction
    glm_vec3_normalize(dir);
    const vec3 pos = {0.0f, 2.0f, 0.0f};
    const vec3 center = {0.0f, 0.0f, 0.0f}; // for SPCameraMode_LookAt

    spInit(&(SPInitDesc){
        .surface_size = {
            .width = surface_width,
            .height = surface_height
        },
        .camera = {
            .pos = {pos[0], pos[1], pos[2]},
            .dir = {dir[0], dir[1], dir[2]},
            .look_at = {center[0], center[1], center[2]},
            .mode = SPCameraMode_Direction,
            .fovy = glm_rad(60.0f),
            .aspect = (float)surface_width / (float) surface_height,
            .near = 0.1f,
            .far = 100.0f // not used
        },
        .pools.capacities = {
            .meshes = 128,
            .materials = 64,
            .render_meshes = 256,
            .lights = 1,
            .scene_nodes = 1024,
        },
        .show_stats = true,
    });

    createObjects();
    start_clock = clock();

    emscripten_set_main_loop(frame, 60, false);
    return 0;
}