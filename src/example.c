#define SPIDER_DEBUG 1
#include "spider.h"

const SPVertex vertices[] = {
    {{-0.5f,  0.0f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // TL
    {{ 0.5f,  0.0f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // TR
    {{ 0.5f,  0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // BR
    {{-0.5f,  0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // BL
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0,
};

#define NORM(x, y, z) {x / 1.732f, y / 1.732f, z / 1.732f}

const SPVertex vertices_box[] = {
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, NORM(-1.0f,  1.0f,  1.0f), {0.0f, 0.0f}}, // LTB 0
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, NORM( 1.0f,  1.0f,  1.0f), {1.0f, 0.0f}}, // RTB 1
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, NORM( 1.0f,  1.0f, -1.0f), {1.0f, 1.0f}}, // RTF 2
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, NORM(-1.0f,  1.0f, -1.0f), {0.0f, 1.0f}}, // LTF 3
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, NORM(-1.0f, -1.0f,  1.0f), {1.0f, 1.0f}}, // LBB 4
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, NORM( 1.0f, -1.0f,  1.0f), {0.0f, 1.0f}}, // RBB 5
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, NORM( 1.0f, -1.0f, -1.0f), {0.0f, 0.0f}}, // RBF 6
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, NORM(-1.0f, -1.0f, -1.0f), {1.0f, 0.0f}}, // LBF 7
};

const uint16_t indices_box[] = {
    0, 1, 2, 2, 3, 0, // top
    3, 2, 6, 6, 7, 3, // front
    0, 3, 7, 7, 4, 0, // left
    1, 0, 4, 4, 5, 1, // back
    2, 1, 5, 5, 6, 2, // right
    6, 5, 4, 4, 7, 6, // bottom
};

#define INSTANCES_COUNT 40
SPInstanceID instance_ids[INSTANCES_COUNT];
SPLightID spot_light_id;
clock_t start_clock;

float randFloat(void) {
    return (float) rand() / (float) RAND_MAX;
}

float randFloatRange(float min, float max) {
    return min + randFloat() * (max - min);
} 

void createObjects(void) {
    SPMeshID mesh = spCreateMesh(&(SPMeshDesc){
            .vertices = {
                .data = vertices,
                .count = ARRAY_LEN(vertices)
            },
            .indices = {
                .data = indices,
                .count = ARRAY_LEN(indices)
            }
        }
    );

    SPMeshID mesh_box = spCreateMesh(&(SPMeshDesc){
            .vertices = {
                .data = vertices_box,
                .count = ARRAY_LEN(vertices_box)
            },
            .indices = {
                .data = indices_box,
                .count = ARRAY_LEN(indices_box)
            }
        }
    );

    // TODO: lights have to be created before materials right now 
    vec3 light_direction = {0.0f, -1.0f, 0.0f};
    glm_vec3_normalize(light_direction);

    spot_light_id = spCreateSpotLight(&(SPSpotLightDesc){
            .pos = {0.0f, 8.0f, 0.0f},
            .range = 20.0f,
            .color = {.r = 255, .g = 255, .b = 255},
            .dir = {light_direction[0], light_direction[1], light_direction[2]},
            .fov = glm_rad(50.0f),
            .power = 20.0f,
            .shadow_casting = &(SPLightShadowCastDesc){
                .shadow_map_size = 1024,
            },
        }
    );
    SPIDER_ASSERT(spot_light_id.id != SP_INVALID_ID);

    SPMaterialID brick = spCreateMaterial(&(SPMaterialDesc){
            .ambient = 0.05f,
            .specular = 0.05f,
            .albedo_tex = {
                .name = "assets/textures/BrickRound0109_1_seamless_S.jpg",
                .width = 1024,
                .height = 1024,
                .channel_count = 3,
            },
        }
    );
    SPMaterialID plastic = spCreateMaterial(&(SPMaterialDesc){
            .ambient = 0.05f,
            .specular = 0.8f,
            .albedo_tex = {
                .name = "assets/textures/Plastic0027_1_seamless_S.jpg",
                .width = 1024,
                .height = 1024,
                .channel_count = 3,
            },
        }
    );

    SPMaterialID gravel = spCreateMaterial(&(SPMaterialDesc){
            .ambient = 0.05f,
            .specular = 0.8f,
            .albedo_tex = {
                .name = "assets/textures/GravelCobble0027_1_seamless_S.jpg",
                .width = 1024,
                .height = 1024,
                .channel_count = 3,
            },
        }
    );
    const float spacing = 4.0f;

    for(int i = 0; i < INSTANCES_COUNT; i++) {
        float scale = randFloatRange(0.5f, 1.0f);
        instance_ids[i] = spCreateInstance(&(SPInstanceDesc){
                .mesh = mesh_box, 
                .material = (rand() % 2) ? plastic : brick,
                .transform = &(SPTransform){
                    .pos = {randFloatRange(-spacing, spacing), randFloatRange(-spacing, spacing), randFloatRange(-spacing, spacing)},
                    .scale = {scale, scale, scale},
                    .rot = {randFloatRange(0.0f, 360.0f), randFloatRange(0.0f, 360.0f), randFloatRange(0.0f, 360.0f)},
                }
            }
        );
    }

    spCreateInstance(&(SPInstanceDesc){
            .mesh = mesh, 
            .material = gravel,
            .transform = &(SPTransform){
                .pos = {0.0f, -spacing, 0.0f},
                .scale = {spacing * 4.0f, 1.0f, spacing * 4.0f},
                .rot = {0.0f, 0.0f, 0.0f},
            }
        }
    );
}

float time_elapsed_total_s = 0.0f;

void frame(void) {
    clock_t cur_clock = clock();
    float delta_time_s = ((float)(cur_clock - start_clock) / CLOCKS_PER_SEC);
    start_clock = clock();
    time_elapsed_total_s += delta_time_s;
    for(int i = 0; i < INSTANCES_COUNT; i++) {
        SPInstance* instance = spGetInstance(instance_ids[i]);
        if(!instance) {
            continue;
        }
        instance->transform.rot[1] += (180.0f / INSTANCES_COUNT) * i * delta_time_s;
        if(instance->transform.rot[1] >= 360.0f) {
            instance->transform.rot[1] -= 360.0f; 
        }
    }
    SPCamera* camera = spGetActiveCamera();
    const float radius = 10.0f;
    const float depth = sin(time_elapsed_total_s * 0.12f) * 6.0f;

    SPLight* spot_light = spGetLight(spot_light_id);
    float angle = sin(glm_rad(30.0f));
    if(spot_light) {
        spot_light->pos[0] = sin(time_elapsed_total_s * 1.0f) * 3.0f;
        spot_light->color = (SPColorRGB8){
            127 + (uint8_t)(sin(time_elapsed_total_s) * 127),
            127 + (uint8_t)(sin(time_elapsed_total_s * 2.3f) * 127),
            127 + (uint8_t)(sin(time_elapsed_total_s * 0.6f) * 54)
        };
    }
    


    spUpdate();
    spRender();
}

int main() {
    srand(0);
    const uint16_t surface_width = 1024;
    const uint16_t surface_height = 768;
    vec3 dir = {0.0f, -1.0f, -1.0f};
    vec3 pos = {0.0f, 5.0f, 8.0f};
    vec3 center = {0.0f, -4.0f, 0.0f};
    //glm_vec3_sub(center, pos, dir);
    glm_vec3_normalize(dir);

    SPInitDesc init = {
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
            .far = 100.0f
        },
        .pools.capacities = {
            .meshes = 8,
            .materials = 8,
            .instances = 4096,
            .lights = 1,
        },
    };
    spInit(&init);
    createObjects();
    start_clock = clock();

    emscripten_set_main_loop(frame, 60, false);
    return 0;
}