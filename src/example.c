#define SPIDER_DEBUG 1
#include "spider.h"

const Vertex vertices[] = {
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // TL
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // TR
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // BR
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}, // BL
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0
};

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

    SPMaterialID material = spCreateMaterial(
        &(SPMaterialDesc){
            .vert.file = "src/shaders/compiled/vert.spv",
            .frag.file = "src/shaders/compiled/frag.spv",
        }
    );
    if(!mesh.id || !material.id) {
        return;
    }
    SPRenderObjectID render_object = spCreateRenderObject(&(SPRenderObjectDesc){
        .mesh = mesh, 
        .material = material
    });
    SPRenderObjectID render_object_2 = spCreateRenderObject(&(SPRenderObjectDesc){
        .mesh = mesh, 
        .material = material
    });

}

void frame(void) {
    spUpdate();
    spRender();
}

int main() {
    const uint16_t surface_width = 640;
    const uint16_t surface_height = 480; 

    spInit(&(SPInitDesc){
        .surface_size = {
            .width = surface_width,
            .height = surface_height
        },
        .camera = {
            .pos = {0.0f, 0.0f, 0.0f},
            .dir = {0.0f, 0.0f, 1.0f},
            .fovy = glm_rad(45.0f),
            .aspect = (float)surface_width / (float) surface_height,
            .near = 0.1f,
            .far = 100.0f
        },
        .pools.capacities = {
            .meshes = 8,
            .materials = 8,
            .render_objects = 8,
        },
    });
    createObjects();
    emscripten_set_main_loop(frame, 30, false);
    return 0;
}