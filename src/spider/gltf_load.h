#ifndef SPIDER_GLTF_LOAD_H_
#define SPIDER_GLTF_LOAD_H_

typedef struct cgltf_data cgltf_data;
typedef struct cgltf_primitive cgltf_primitive;
typedef struct cgltf_material cgltf_material;

#include "mesh.h"
#include "material.h"
#include "scene_node.h"

/*
Loads mesh and material from a .gltf file, creates respective mesh and material from the data and 
returns an object with identifiers to the mesh/material
*/
SPSceneNodeID spLoadGltf(const char* file);

/*
Creates a Mesh from the given cgltf_primitive and returns an ID to it
It expects the buffers to be loaded
*/
SPMeshID _spLoadMeshPrimitiveFromGltf(const cgltf_primitive* prim, const char* gltf_path);
/*
Creates a Material from the given cgltf_material and returns an ID to it
Loads the images itself (but doesn't cache them)
*/
SPMaterialID _spLoadMaterialFromGltf(const cgltf_material* mat, const char* gltf_path);

/*
Combines two paths and saves it in result (used for loading gltf materials that store images relative to the base file)
Example:
base: foo/bar/xyz.txt
new: abc.bin
result: foo/bar/abc.bin
*/
void _spModifyRelativeFilePath(const char* base_path, const char* new_path, char* result);

#endif // SPIDER_GLTF_LOAD_H_