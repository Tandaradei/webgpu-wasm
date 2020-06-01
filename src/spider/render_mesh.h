#ifndef SPIDER_RENDER_MESH_H_
#define SPIDER_RENDER_MESH_H_

#include "mesh.h"
#include "material.h"

typedef struct SPRenderMesh {
    SPMeshID mesh_id;
    SPMaterialID material_id;
    SPMesh* _mesh;
    SPMaterial* _material;
} SPRenderMesh;

typedef struct SPRenderMeshID {
    uint32_t id;
} SPRenderMeshID;

typedef struct SPRenderMeshDesc {
    SPMeshID mesh;
    SPMaterialID material;
} SPRenderMeshDesc;

/*
Creates a render mesh and returns an identifier to it
*/
SPRenderMeshID spCreateRenderMesh(const SPRenderMeshDesc* desc);

#endif // SPIDER_RENDER_MESH_H_