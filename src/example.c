#include <emscripten/emscripten.h>
#include "spider/spider.h"

vec3 plane_vertices[] = {
    {-0.5f,  0.0f,  0.5f},
    { 0.5f,  0.0f,  0.5f},
    { 0.5f,  0.0f, -0.5f},
    {-0.5f,  0.0f, -0.5f},
};

vec2 plane_tex_coords[] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f} 
};

SPTriangle plane_faces[] = {
    {
        .vertex_indices = {0, 1, 2},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {2, 3, 0},
        .tex_coord_indices = {2, 3, 0}
    },
};

const SPMeshInitializer plane = {
    .vertices = {
        .data = plane_vertices,
        .count = ARRAY_LEN(plane_vertices),
    },
    .tex_coords = {
        .data = plane_tex_coords,
        .count = ARRAY_LEN(plane_tex_coords),
    },
    .faces = {
        .data = plane_faces,
        .count = ARRAY_LEN(plane_faces)
    }
};

vec3 cube_vertices[] = {
    {-0.5f,  0.5f,  0.5f}, // LTB 0
    { 0.5f,  0.5f,  0.5f}, // RTB 1
    { 0.5f,  0.5f, -0.5f}, // RTF 2
    {-0.5f,  0.5f, -0.5f}, // LTF 3
    {-0.5f, -0.5f,  0.5f}, // LBB 4
    { 0.5f, -0.5f,  0.5f}, // RBB 5
    { 0.5f, -0.5f, -0.5f}, // RBF 6
    {-0.5f, -0.5f, -0.5f}, // LBF 7
};

vec2 cube_tex_coords[] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
};

SPTriangle cube_faces[] = {
    // Top
    {
        .vertex_indices = {0, 1, 2},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {2, 3, 0},
        .tex_coord_indices = {2, 3, 0}
    },
    // Front
    {
        .vertex_indices = {1, 0, 4},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {4, 5, 1},
        .tex_coord_indices = {2, 3, 0}
    },
    // Left
    {
        .vertex_indices = {0, 3, 7},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {7, 4, 0},
        .tex_coord_indices = {2, 3, 0}
    },
    // Back
    {
        .vertex_indices = {3, 2, 6},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {6, 7, 3},
        .tex_coord_indices = {2, 3, 0}
    },
    // Right
    {
        .vertex_indices = {2, 1, 5},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {5, 6, 2},
        .tex_coord_indices = {2, 3, 0}
    },
    // Bottom
    {
        .vertex_indices = {6, 5, 4},
        .tex_coord_indices = {0, 1, 2}
    },
    {
        .vertex_indices = {4, 7, 6},
        .tex_coord_indices = {2, 3, 0}
    },
};

const SPMeshInitializer cube = {
    .vertices = {
        .data = cube_vertices,
        .count = ARRAY_LEN(cube_vertices),
    },
    .tex_coords = {
        .data = cube_tex_coords,
        .count = ARRAY_LEN(cube_tex_coords),
    },
    .faces = {
        .data = cube_faces,
        .count = ARRAY_LEN(cube_faces)
    }
};

SPLightID spot_light_id;
clock_t start_clock;

float randFloat(void) {
    return (float) rand() / (float) RAND_MAX;
}

float randFloatRange(float min, float max) {
    return min + randFloat() * (max - min);
}

static const vec3 light_start_pos = {0.0f, 10.0f, 10.0f};
SPInstanceID avocado_instance_id = {SP_INVALID_ID};
SPInstanceID fish_instance_id = {SP_INVALID_ID};

void createObjects(void) {
    SPMeshID mesh = spCreateMeshFromInit(&plane);
    SPMeshID mesh_cube = spCreateMeshFromInit(&cube);
    
    // TODO: lights have to be created before materials right now 
    vec3 light_look_at = {0.0f, 0.0f, 0.0f};
    vec3 light_direction = {-1.0, -1.0f, 0.2f};
    // (float*) cast to prevent compiler warning 'incompatible-pointer-types-discards-qualifiers'
    // cglm takes no const pointers as arguments, even if it doesn't mutate the vectors
    glm_vec3_sub(light_look_at, (float*)light_start_pos, light_direction);
    glm_vec3_normalize(light_direction);

    spot_light_id = spCreateSpotLight(&(SPSpotLightDesc){
            .pos = {light_start_pos[0], light_start_pos[1], light_start_pos[2]},
            .range = 40.0f,
            .color = {.r = 255, .g = 255, .b = 255},
            .dir = {light_direction[0], light_direction[1], light_direction[2]},
            .fov = glm_rad(70.0f),
            .power = 100.0f,
            .shadow_casting = &(SPLightShadowCastDesc){
                .shadow_map_size = 1024,
            },
        }
    );
    SPIDER_ASSERT(spot_light_id.id != SP_INVALID_ID);

    SPObject ground_object = spLoadGltf("assets/gltf/ManholeCover/ManholeCover.gltf");
    if(ground_object.mesh.id != SP_INVALID_ID && ground_object.material.id != SP_INVALID_ID) {
        spCreateInstance(&(SPInstanceDesc){
            .object = ground_object,
            .transform = &(SPTransform){
                .pos = {0.0f, 0.0f, 0.0f},
                .scale = {10.0f, 1.0f, 10.0f},
                .rot = {0.0f, 0.0f, 0.0f},
            }
        });
    }

    SPObject avocado_object = spLoadGltf("assets/gltf/Avocado/Avocado.gltf");
    if(avocado_object.mesh.id != SP_INVALID_ID && avocado_object.material.id != SP_INVALID_ID) {
        avocado_instance_id = spCreateInstance(&(SPInstanceDesc){
            .object = avocado_object,
            .transform = &(SPTransform){
                .pos = {-3.0f, -1.0f, 0.0f},
                .scale = {50.0f, 50.0f, 50.0f},
                .rot = {0.0f, 0.0f, 0.0f},
            }
        });
    }

    SPObject fish_object = spLoadGltf("assets/gltf/BarramundiFish/BarramundiFish.gltf");
    if(fish_object.mesh.id != SP_INVALID_ID && fish_object.material.id != SP_INVALID_ID) {
        fish_instance_id = spCreateInstance(&(SPInstanceDesc){
            .object = fish_object,
            .transform = &(SPTransform){
                .pos = {3.0f, -1.0f, 0.0f},
                .scale = {10.0f, 10.0f, 10.0f},
                .rot = {0.0f, 200.0f, 0.0f},
            }
        });
    }
}

float time_elapsed_total_s = 0.0f;

void frame(void) {
    clock_t cur_clock = clock();
    float delta_time_s = ((float)(cur_clock - start_clock) / CLOCKS_PER_SEC);
    start_clock = clock();
    time_elapsed_total_s += delta_time_s;

    // Update avocado transform
    /*
    SPInstance* avocado = spGetInstance(avocado_instance_id);
    if(avocado) {
        avocado->transform.rot[1] += -80.0f * delta_time_s;
    }
    // Update plane transform
    SPInstance* fish = spGetInstance(fish_instance_id);
    if(fish) {
        fish->transform.rot[1] += 10.0f * delta_time_s;
    }
    */
    // Update light(s)
    bool should_animate_light = true;
    if(should_animate_light) {
        SPLight* spot_light = spGetLight(spot_light_id);
        if(spot_light) {
            float angle = sin(glm_rad(30.0f));
            float light_distance_from_center = 12.0f;
            float rounds_per_second = 0.25f;
            float rotation_speed = rounds_per_second * M_PI;

            spot_light->pos[0] = sin(time_elapsed_total_s * rotation_speed) * light_distance_from_center;
            //spot_light->pos[1] = light_start_pos[1] + sin(time_elapsed_total_s * 0.4f) * 2.0f;
            spot_light->pos[2] = cos(time_elapsed_total_s * rotation_speed) * light_distance_from_center;
            vec3 light_look_at = {0.0f, 0.0f, 0.0f};
            vec3 light_direction = {-1.0, -1.0f, 0.2f};
            glm_vec3_sub(light_look_at, spot_light->pos, light_direction);
            glm_vec3_normalize(light_direction);
            memcpy(spot_light->dir, light_direction, sizeof spot_light->dir);
        }
    }

    spUpdate();
    spRender();
}

int main() {
    srand(0);
    const uint16_t surface_width = 1280;
    const uint16_t surface_height = 720;
    vec3 dir = {0.0f, -1.0f, -1.0f}; // for SPCameraMode_Direction
    glm_vec3_normalize(dir);
    vec3 pos = {0.0f, 2.0f, 8.0f};
    vec3 center = {0.0f, 0.0f, 0.0f}; // for SPCameraMode_LookAt

    spInit(&(SPInitDesc){
        .surface_size = {
            .width = surface_width,
            .height = surface_height
        },
        .camera = {
            .pos = {pos[0], pos[1], pos[2]},
            .dir = {dir[0], dir[1], dir[2]},
            .look_at = {center[0], center[1], center[2]},
            .mode = SPCameraMode_LookAt,
            .fovy = glm_rad(60.0f),
            .aspect = (float)surface_width / (float) surface_height,
            .near = 0.1f,
            .far = 100.0f // not used
        },
        .pools.capacities = {
            .meshes = 8,
            .materials = 8,
            .instances = 4096,
            .lights = 1,
        },
    });
    createObjects();
    start_clock = clock();

    emscripten_set_main_loop(frame, 60, false);
    return 0;
}