#ifndef SPIDER_SCENE_NODE_H_
#define SPIDER_SCENE_NODE_H_

#include <cglm/cglm.h>
#include <stdint.h>

#include "render_mesh.h"
#include "light.h"

typedef struct SPTransform {
    vec3 pos; // 12 bytes
    vec3 rot; // 12 bytes
    vec3 scale; // 12 bytes
} SPTransform; // 36 bytes

typedef enum SPSceneNodeType {
    SPSceneNodeType_Empty,
    SPSceneNodeType_RenderMesh,
    SPSceneNodeType_Light,
    SPSceneNodeType_Force32 = 0xFFFFFFFF
} SPSceneNodeType;

typedef struct SPSceneNodeID {
    uint32_t id;
} SPSceneNodeID;

/**
 * If each node saves it's parent also as an ID, the whole tree can be 
 * reconstructed (at deserialization for example), 
 * but we don't have the disadvantage of a ID lookup at runtime for children
 */
typedef struct _SPSceneNodeTree {
    struct SPSceneNode* parent;
    SPSceneNodeID _parent_id; // for serialization
    struct {
        union {
            struct SPSceneNode* single;
            struct SPSceneNode** list;
        };
        uint32_t count;
        uint32_t capacity;
    } children;
} _SPSceneNodeTree;

typedef struct SPSceneNode {
    _SPSceneNodeTree tree;

    // TODO: transform (+ _transform_world) could maybe be a SOA
    // relative transform to parent
    SPTransform transform;
    // cached evaluated transform matrix in world space
    // TODO: needs to be revisited (bad for dynamic objects which 
    // are changing each frame)
    mat4 _transform_world;

    struct {
        SPSceneNodeType type;
        union {
            struct {
                uint32_t id; // not necessary, but mimics other IDs
            } empty;
            SPRenderMeshID render_mesh; 
            SPLightID light; 
        };
    } linked_object;
} SPSceneNode;

typedef struct SPEmptySceneNodeDesc {
    const SPTransform* transform;
    SPSceneNodeID parent;
} SPEmptySceneNodeDesc;
/*
Creates a SceneNode with no linked object from the given properties 
and if succesfull, returns a valid ID
*/
SPSceneNodeID spCreateEmptySceneNode(const SPEmptySceneNodeDesc* desc);

typedef struct SPRenderMeshSceneNodeDesc {
    SPMeshID mesh;
    SPMaterialID material;
    const SPTransform* transform;
    SPSceneNodeID parent;
} SPRenderMeshSceneNodeDesc;
/*
Creates a SceneNode with a linked RenderMesh (which will be created from mesh + material)
and if succesfull, returns a valid ID
*/
SPSceneNodeID spCreateRenderMeshSceneNode(const SPRenderMeshSceneNodeDesc* desc);

typedef struct SPLightSceneNodeDesc {
    SPLightID light;
    const SPTransform* transform;
    SPSceneNodeID parent;
} SPLightSceneNodeDesc;
/*
Creates a SceneNode with a linked Light
and if succesfull, returns a valid ID
*/
SPSceneNodeID spCreateLightSceneNode(const SPLightSceneNodeDesc* desc);

// TODO: add description
void spSceneNodeSetParent(SPSceneNode* node, SPSceneNode* parent);
// TODO: add description
void spSceneNodeAddChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count);
/*
Shortcut for adding just one child, calls spSceneNodeAddChildren internally
*/
void spSceneNodeAddChild(SPSceneNode* node, SPSceneNode* child);
// TODO: add description
void spSceneNodeRemoveChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count);
/*
Shortcut for removing just one child, calls spSceneNodeAddChildren internally
*/
void spSceneNodeRemoveChild(SPSceneNode* node, SPSceneNode* child);
// TODO: add description
void spSceneNodeSetChildrenCapacity(SPSceneNode* node, uint32_t capacity);
/*
Shortcut for setting the capacity only if it's greater then already set
*/
void spSceneNodeIncreaseChildrenCapacityTo(SPSceneNode* node, uint32_t capacity);

// TODO: add description
void spSceneNodeMarkDirty(SPSceneNode* node);

// PRIVATE
typedef struct _SPSceneNodeDesc {
    const SPTransform* transform;
    SPSceneNode* parent;
    SPSceneNodeType type;
    uint32_t linked_id;
} _SPSceneNodeDesc;
// TODO: add description
SPSceneNodeID _spCreateSceneNode(const _SPSceneNodeDesc* desc);
// TODO: add description
void _spSceneNodeUpdateWorldTransform(SPSceneNode* node);



#endif // SPIDER_SCENE_NODE_H_