#include "spider/spider.h"
#include "mesh_data.h"

#define SHEETS 19

static SPLightID spot_light_id;
static uint32_t last_mouse_pos_x = 0;
static uint32_t last_mouse_pos_y = 0;
static const uint32_t surface_width = 1280;
static const uint32_t surface_height = 720;
static vec3 cam_rot = {0.0f, 0.0f, 0.0f};
static vec4 forward = {0.0f, 0.0f, 1.0f, 0.0f};
static float sensitivity = 2.0f;
static float vertical_limit = 0.01f;

static SPSceneNodeID cube_root;
static SPSceneNodeID sheets[SHEETS];

void init(void) {
    // Lights have to be created before materials right now 
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

    /*SPSceneNodeID sponza_node_id = */spLoadGltf("assets/gltf/Sponza/Sponza.gltf");

    cube_root = spCreateEmptySceneNode(&(SPEmptySceneNodeDesc){
        .transform = &(SPTransform) {
            .pos = {0.0f, 0.0f, 0.0f},
            .rot = {0.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        },
        .parent = {SP_INVALID_ID}
    });

    SPMeshID cube_mesh = spCreateMeshFromInit(&cube);

    const float cube_scale = 1.0f;
    spSceneNodeSetChildrenCapacity(spGetSceneNode(cube_root), SHEETS);
    for(uint32_t i = 0; i < SHEETS; i++) {
        char name[50];
        sprintf(name, "assets/textures/kolloquium/kolloquium-%02d.jpg", i+1);
        printf("Loading %s\n", name);
        SPMaterialID mat_id = spCreateMaterial(&(SPMaterialDesc){
            .albedo = name,
            .normal = "assets/textures/Metal003_2K_Normal.jpg",
            .ao_roughness_metallic = "assets/textures/cube_arm.png"
        });
        sheets[i] = spCreateRenderMeshSceneNode(&(SPRenderMeshSceneNodeDesc){
            .material = mat_id,
            .mesh = cube_mesh,
            .transform = &(SPTransform){
                .pos = {-(SHEETS * 0.5f * cube_scale) + i * cube_scale, 1.0f, 0.0f},
                .scale = {cube_scale * 0.5f, cube_scale * 0.5f, cube_scale * 0.5f}
            },
            .parent = cube_root
        });
    }
}

bool update(float delta_time_s) {
    static bool show_controls = true;
    igBegin("Controls", &show_controls, ImGuiWindowFlags_None);
        igText("Look:");
            igBulletText("Hold right mouse button and move mouse");
        igText("Move:");
            igBulletText("W: Forward");
            igBulletText("S: Back");
            igBulletText("A: Left");
            igBulletText("D: Right");
            igBulletText("Space: Up");
            igBulletText("LeftControl: Down");
    igEnd();

    static bool scene_control = true;
    igBegin("Scene control", &scene_control, ImGuiWindowFlags_None);
    
    SPCamera* cam = spGetActiveCamera();
    glm_vec4_normalize(forward);

    if(cam) {
        if(spGetMouseButtonPressed(SPMouseButton_Right)) {
            vec2 relative_delta = {
                ((float)spGetMousePositionX() - (float)last_mouse_pos_x) / (float) surface_width,
                ((float)spGetMousePositionY() - (float)last_mouse_pos_y) / (float) surface_height
            };
            float rotation_speed = sensitivity * M_PI;
            cam_rot[1] -= rotation_speed * relative_delta[0]; // horizontal
            cam_rot[0] += rotation_speed * relative_delta[1]; // vertical
            cam_rot[0] = glm_clamp(cam_rot[0], (-M_PI * 0.5f) + vertical_limit, (M_PI * 0.5f) - vertical_limit);
        }
        memcpy(forward, (vec4){0.0f, 0.0f, 1.0f, 0.0f}, sizeof(vec4));
        mat4 rot = GLM_MAT4_IDENTITY_INIT;
        glm_euler_zyx(cam_rot, rot);
        glm_mat4_mulv(rot, forward, forward);

        cam->dir[0] = forward[0];
        cam->dir[1] = forward[1];
        cam->dir[2] = forward[2];
        vec3 sideward = {
            -forward[2],
            0.0f,
            forward[0],
        };
        glm_vec3_normalize(sideward);
        const float walk_speed = 2.0f;
        const float forward_movement = walk_speed * delta_time_s * (-1.0f * spGetKeyPressed(SPKey_S) + spGetKeyPressed(SPKey_W));
        const float sideward_movement = walk_speed * delta_time_s * (-1.0f * spGetKeyPressed(SPKey_A) + spGetKeyPressed(SPKey_D));
        const float upward_movement = walk_speed * delta_time_s * (-1.0f * spGetKeyPressed(SPKey_ControlLeft) + spGetKeyPressed(SPKey_Space));
        cam->pos[0] += forward[0] * forward_movement + sideward[0] * sideward_movement;
        cam->pos[1] += forward[1] * forward_movement + upward_movement;
        cam->pos[2] += forward[2] * forward_movement + sideward[2] * sideward_movement;
        
        if(igCollapsingHeaderTreeNodeFlags("Camera", ImGuiTreeNodeFlags_None)) {
            igSliderFloat("Look sensitivity##cam", &sensitivity, 0.0f, 5.0f, "%.1f", 1.0f);
            igSliderFloat("Vertical look limit##cam", &vertical_limit, 0.0f, M_PI * 0.5f, "%.2f", 1.0f);
            igSliderFloat3("Position##cam", (float*)&cam->pos, -10.0f, 10.0f, "%.2f", 1.0f);
            igSliderFloat3("Rotation (Rad)##cam", (float*)&cam_rot, -M_PI, M_PI, "%.2f", 1.0f);
            igSliderFloat("Vertical field of view (Rad)##cam", &cam->fovy, 0.01f, M_PI, "%.2f", 1.0f);
        }
    }

    last_mouse_pos_x = spGetMousePositionX();
    last_mouse_pos_y = spGetMousePositionY();
    
    SPLight* spot_light = spGetLight(spot_light_id);
    if(spot_light) {
        if(igCollapsingHeaderTreeNodeFlags("Spotlight", ImGuiTreeNodeFlags_None)) {
            igSliderFloat3("Position##light", (float*)&spot_light->pos, -50.0f, 50.0f, "%.1f", 1.0f);
            igSliderFloat("Field of view##light", &spot_light->fov, 0.0f, M_PI, "%.2f", 1.0f);
            igSliderFloat("Power##light", &spot_light->power, 0.0f, 1000.0f, "%.0f", 1.0f);
            igSliderFloat("Range##light", &spot_light->range, 0.0f, 1000.0f, "%.0f", 1.0f);
        }
    }
    igEnd();

    const float rotation_speed = 20.0f * M_PI / 180.0f;
    for(uint32_t i = 0; i < SHEETS; i++) {
        SPSceneNode* node = spGetSceneNode(sheets[i]);
        if(node) {
            node->transform.rot[1] += i * rotation_speed * delta_time_s;
        }
    }
    spSceneNodeMarkDirty(spGetSceneNode(cube_root));

    // return false if you want to quit
    return true;
}

int main() {
    spInit(&(SPInitDesc){
        .surface_size = {
            .width = surface_width,
            .height = surface_height
        },
        .update_func = update,
        .camera = {
            .pos = {0.0f, 2.0f, 0.0f},
            .dir = {0.0f, 0.0f, 1.0f},
            .look_at = {0.0f, 0.0f, 0.0f},
            .mode = SPCameraMode_Direction,
            .fovy = glm_rad(60.0f),
            .aspect = (float)surface_width / (float) surface_height,
            .near = 0.1f,
        },
        .pools.capacities = {
            .meshes = 128,
            .materials = 128,
            .render_meshes = 512,
            .lights = 1,
            .scene_nodes = 1024,
        },
        .show_stats = true,
    });

    init();

    spStart();
    return 0;
}