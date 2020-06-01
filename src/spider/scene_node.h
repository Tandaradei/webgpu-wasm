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

    SPTransform transform; // relative transform to parent
    mat4 _transform_world; // cached evaluated transform matrix in world space

    struct {
        SPSceneNodeType type;
        union {
            struct {
                uint32_t id;
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
SPSceneNodeID spCreateEmptySceneNode(const SPEmptySceneNodeDesc* desc);

typedef struct SPRenderMeshSceneNodeDesc {
    SPMeshID mesh;
    SPMaterialID material;
    const SPTransform* transform;
    SPSceneNodeID parent;
} SPRenderMeshSceneNodeDesc;
SPSceneNodeID spCreateRenderMeshSceneNode(const SPRenderMeshSceneNodeDesc* desc);

typedef struct SPLightSceneNodeDesc {
    SPLightID light;
    const SPTransform* transform;
    SPSceneNodeID parent;
} SPLightSceneNodeDesc;
SPSceneNodeID spCreateLightSceneNode(const SPLightSceneNodeDesc* desc);

void spSceneNodeSetParent(SPSceneNode* node, SPSceneNode* parent);
void spSceneNodeAddChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count);
void spSceneNodeAddChild(SPSceneNode* node, SPSceneNode* child);
void spSceneNodeRemoveChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count);
void spSceneNodeRemoveChild(SPSceneNode* node, SPSceneNode* child);
void spSceneNodeSetChildrenCapacity(SPSceneNode* node, uint32_t capacity);
void spSceneNodeIncreaseChildrenCapacityTo(SPSceneNode* node, uint32_t capacity);

void spSceneNodeMarkDirty(SPSceneNode* node);

// PRIVATE
typedef struct _SPSceneNodeDesc {
    const SPTransform* transform;
    SPSceneNode* parent;
    SPSceneNodeType type;
    uint32_t linked_id;
} _SPSceneNodeDesc;
SPSceneNodeID _spCreateSceneNode(const _SPSceneNodeDesc* desc);
void _spSceneNodeUpdateWorldTransform(SPSceneNode* node);



#endif // SPIDER_SCENE_NODE_H_