#define SPIDER_DEBUG 1
#include "spider.h"

const Vertex vertices[] = {
    {{-0.5f,  0.0f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // TL
    {{ 0.5f,  0.0f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // TR
    {{ 0.5f,  0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // BR
    {{-0.5f,  0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // BL
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0,
};

#define NORM(x, y, z) {x / 1.732f, y / 1.732f, z / 1.732f}

const Vertex vertices_box[] = {
    {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, NORM(-1.0f,  1.0f,  1.0f)}, // LTB 0
    {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, NORM( 1.0f,  1.0f,  1.0f)}, // RTB 1
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, NORM( 1.0f,  1.0f, -1.0f)}, // RTF 2
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, NORM(-1.0f,  1.0f, -1.0f)}, // LTF 3
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, NORM(-1.0f, -1.0f,  1.0f)}, // LBB 4
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}, NORM( 1.0f, -1.0f,  1.0f)}, // RBB 5
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, NORM( 1.0f, -1.0f, -1.0f)}, // RBF 6
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, NORM(-1.0f, -1.0f, -1.0f)}, // LBF 7
};

const uint16_t indices_box[] = {
    0, 1, 2, 2, 3, 0, // top
    3, 2, 6, 6, 7, 3, // front
    0, 3, 7, 7, 4, 0, // left
    1, 0, 4, 4, 5, 1, // back
    2, 1, 5, 5, 6, 2, // right
    6, 5, 4, 4, 7, 6, // bottom
};

#define INSTANCES_COUNT 64
SPInstanceID instance_ids[INSTANCES_COUNT];
SPInstanceID look_at_cube_id;
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

    SPMaterialID rough = spCreateMaterial(&(SPMaterialDesc){
            .specular = 0.05f,
        }
    );
    SPMaterialID shiny = spCreateMaterial(&(SPMaterialDesc){
            .specular = 0.8f,
        }
    );

    if(!mesh.id || !rough.id || !shiny.id) {
        return;
    }
    
    const float spacing = 4.0f;

    for(int i = 0; i < INSTANCES_COUNT; i++) {
        instance_ids[i] = spCreateInstance(&(SPInstanceDesc){
                .mesh = mesh_box, 
                .material = (rand() % 2) ? shiny : rough,
                .transform = &(SPTransform){
                    .pos = {randFloatRange(-spacing, spacing), randFloatRange(-spacing, spacing), randFloatRange(-spacing, spacing)},
                    .scale = {randFloatRange(0.5f, 1.5f), randFloatRange(0.5f, 1.5f), randFloatRange(0.5f, 1.5f)},
                    .rot = {randFloatRange(0.0f, 360.0f), randFloatRange(0.0f, 360.0f), randFloatRange(0.0f, 360.0f)},
                }
            }
        );
    }

    spCreateInstance(&(SPInstanceDesc){
            .mesh = mesh, 
            .material = rough,
            .transform = &(SPTransform){
                .pos = {0.0f, -spacing, 0.0f},
                .scale = {spacing * 2.0f, 1.0f, spacing * 2.0f},
                .rot = {0.0f, 0.0f, 0.0f},
            }
        }
    );
    look_at_cube_id = spCreateInstance(&(SPInstanceDesc){
            .mesh = mesh_box, 
            .material = shiny,
            .transform = &(SPTransform){
                .pos = {0.0f, 0.0f, 0.0f},
                .scale = {0.2f, 0.2f, 0.2f},
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
        instance->transform.rot[1] += 2.0f * i * delta_time_s;
        if(instance->transform.rot[1] >= 360.0f) {
            instance->transform.rot[1] -= 360.0f; 
        }
    }
    SPCamera* camera = spGetActiveCamera();
    const float radius = 10.0f;
    const float depth = sin(time_elapsed_total_s * 0.12f) * 6.0f;

    memcpy(
        camera->look_at, 
        (vec3){
            sin(time_elapsed_total_s * 0.49f) * 2.0f, 
            sin(23.5f + time_elapsed_total_s * 0.27f), 
            depth,
        }, 
        sizeof(vec3)
    );

    SPInstance* look_at_cube = spGetInstance(look_at_cube_id);
    memcpy(look_at_cube->transform.pos, camera->look_at, sizeof(vec3));
    spSetLightPos(camera->look_at);

    camera->fovy = glm_rad(60.0f - depth * 5.0f);
    //memcpy(camera->pos, (vec3){sin(time_elapsed_total_s) * radius, 0.0f, cos(time_elapsed_total_s) * -radius}, sizeof(vec3));
    spUpdate();
    spRender();
}

int main() {
    srand(0);
    const uint16_t surface_width = 1280;
    const uint16_t surface_height = 720;
    vec3 dir = {0.0f, 0.0f, 1.0f};
    vec3 pos = {0.0f, 0.0f, -10.0f};
    vec3 center = {0.0f, 0.0f, 0.0f};
    glm_vec3_sub(center, pos, dir);
    glm_vec3_norm(dir);

    SPInitDesc init = {
        .surface_size = {
            .width = surface_width,
            .height = surface_height
        },
        .camera = {
            .pos = {pos[0], pos[1], pos[2]},
            .dir = {dir[0], dir[1], dir[2]},
            .look_at = {center[0], center[1], center[3]},
            .mode = SPCameraMode_LookAt,
            .fovy = glm_rad(60.0f),
            .aspect = (float)surface_width / (float) surface_height,
            .near = 0.1f,
            .far = 100.0f
        },
        .pools.capacities = {
            .meshes = 8,
            .materials = 8,
            .instances = 256,
        },
    };
    spInit(&init);
    createObjects();
    start_clock = clock();
    emscripten_set_main_loop(frame, 60, false);
    return 0;
}