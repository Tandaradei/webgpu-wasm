#include "scene_node.h"

#include "render_mesh.h"
#include "impl.h"
#include "debug.h"
#include "state.h"

extern _SPState _sp_state;


SPSceneNodeID spCreateEmptySceneNode(const SPEmptySceneNodeDesc* desc) {
    return _spCreateSceneNode(&(_SPSceneNodeDesc){
        .parent = spGetSceneNode(desc->parent),
        .transform = desc->transform,
        .type = SPSceneNodeType_Empty,
        .linked_id = 0
    });
}

SPSceneNodeID spCreateRenderMeshSceneNode(const SPRenderMeshSceneNodeDesc* desc) {
    return _spCreateSceneNode(&(_SPSceneNodeDesc){
        .parent = spGetSceneNode(desc->parent),
        .transform = desc->transform,
        .type = SPSceneNodeType_RenderMesh,
        .linked_id = spCreateRenderMesh(&(SPRenderMeshDesc){
            .mesh = desc->mesh,
            .material = desc->material,
        }).id
    });
}

SPSceneNodeID spCreateLightSceneNode(const SPLightSceneNodeDesc* desc) {
    return _spCreateSceneNode(&(_SPSceneNodeDesc){
        .parent = spGetSceneNode(desc->parent),
        .transform = desc->transform,
        .type = SPSceneNodeType_Light,
        .linked_id = desc->light.id
    });
}


void spSceneNodeSetParent(SPSceneNode* node, SPSceneNode* parent) {
    SP_ASSERT(node && node != parent);
    
    if(parent) {
        spSceneNodeAddChild(parent, node); // add node to new parent (also removes from old parent)
    }
    else if(node->tree.parent){
        spSceneNodeRemoveChild(node->tree.parent, node);  // remove node from old parent
    }
    else {
        node->tree.parent = NULL; // just update the parent property
    }
}

void spSceneNodeAddChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count) {
    SP_ASSERT(node && children && count > 0);
    for(uint32_t i = 0; i < count; i++) {
        SP_ASSERT(children[i]);
        if(children[i]->tree.parent) {
            spSceneNodeRemoveChild(node->tree.parent, node);
        }
        children[i]->tree.parent = node;
    }
    uint32_t new_count = node->tree.children.count + count;
    spSceneNodeIncreaseChildrenCapacityTo(node, new_count);
    if(node->tree.children.capacity == 1 && node->tree.children.count == 0 && count == 1) {
        node->tree.children.single = children[0]; // insert single node
    }
    else {
        memcpy(&node->tree.children.list[node->tree.children.count], children, count * sizeof (SPSceneNode*)); // insert new nodes
    }
    DEBUG_PRINT(DEBUG_PRINT_WARNING, "Setting child count from %d to %d\n", node->tree.children.count, new_count);
    node->tree.children.count = new_count;
}

void spSceneNodeAddChild(SPSceneNode* node, SPSceneNode* child) {
    SP_ASSERT(node && child);
    spSceneNodeAddChildren(node, &child, 1);
}

void spSceneNodeRemoveChildren(SPSceneNode* node, SPSceneNode** children, uint32_t count) {
    SP_ASSERT(node && children && count > 0);
    switch (node->tree.children.count) {
        case 0:
            return;
            break;
        case 1:
            for(uint32_t i = 0; i < count && node->tree.children.count > 0; i++) {
                if(node->tree.children.single == children[i]) {
                    node->tree.children.single->tree.parent = NULL;
                    node->tree.children.single = NULL;
                    node->tree.children.count = 0;
                }
            }
            break;
        default:
            break;
    }
    for(uint32_t i = 0; i < count && node->tree.children.count > 0; i++) {
        for(uint32_t u = 0; u < node->tree.children.count; u++) {
            if(node->tree.children.list[u] == children[i]) {
                    node->tree.children.single->tree.parent = NULL;
                    node->tree.children.list[u] = node->tree.children.list[node->tree.children.count - 1]; // move last child node to removed index
                    node->tree.children.list[node->tree.children.count - 1] = NULL;
                    node->tree.children.count--;
                }
        }
    }
}

void spSceneNodeRemoveChild(SPSceneNode* node, SPSceneNode* child) {
    SP_ASSERT(node && child);
    spSceneNodeRemoveChildren(node, &child, 1);
}

void spSceneNodeSetChildrenCapacity(SPSceneNode* node, uint32_t capacity) {
    SP_ASSERT(node);
    SP_ASSERT(capacity >= node->tree.children.count);
    if(capacity == 0 || capacity == 1 || capacity == node->tree.children.capacity) {
        node->tree.children.capacity = capacity;
        return;
    }
    else {
        switch (node->tree.children.capacity) {
            case 0: {
                node->tree.children.list = SP_MALLOC(capacity * sizeof (*node->tree.children.list));
                break;
            }
            case 1: {
                SPSceneNode* single_node = node->tree.children.single;
                node->tree.children.list = SP_MALLOC(capacity * sizeof (*node->tree.children.list));
                node->tree.children.list[0] = single_node; // insert single node into list
                break;
            }
            default: {
                SPSceneNode** old_list = node->tree.children.list;
                node->tree.children.list = SP_MALLOC(capacity * sizeof (*node->tree.children.list));
                memcpy(&node->tree.children.list[0], old_list, node->tree.children.count); // insert old nodes
                free(old_list);
                break;
            }
        }
        node->tree.children.capacity = capacity;
    }
}

void spSceneNodeIncreaseChildrenCapacityTo(SPSceneNode* node, uint32_t capacity) {
    if(capacity > node->tree.children.capacity) {
        spSceneNodeSetChildrenCapacity(node, capacity);
    }
}

void spSceneNodeMarkDirty(SPSceneNode* node) {
    SP_ASSERT(node);
    bool should_add = true;
    // check if either the node itself or one of it's parents is in the list
    for(uint32_t i = 0; i < _sp_state.dirty_nodes.count; i++) {
        SPSceneNode* to_test = node;
        while(to_test) {
            if(_sp_state.dirty_nodes.data[i] == to_test) {
                should_add = false;
                break;
            }
            to_test = to_test->tree.parent;
        }
    }
    if(should_add) {
        _sp_state.dirty_nodes.data[_sp_state.dirty_nodes.count++] = node;
    }
    // TODO: if a child of a node, that's already in the list, is marked dirty
    // they will be both in the list and the child and all it's children will
    // be updated multiple times
}

SPSceneNodeID _spCreateSceneNode(const _SPSceneNodeDesc* desc) {
    SPSceneNodeID node_id = (SPSceneNodeID){_spAllocPoolIndex(&(_sp_state.pools.scene_node.info))};
    if(node_id.id == SP_INVALID_ID) {
        return node_id;
    }
    uint32_t id = node_id.id; 
    SPSceneNode* node = &(_sp_state.pools.scene_node.data[id]);
    if(desc->parent) {
        spSceneNodeSetParent(node, desc->parent);
    }
    node->tree.children.capacity = 0;
    node->tree.children.count = 0;
    node->tree.children.single = NULL;
    memcpy(&(node->transform), desc->transform, sizeof(SPTransform));
    node->linked_object.type = desc->type;
    switch (node->linked_object.type)
    {
        case SPSceneNodeType_RenderMesh:
            node->linked_object.render_mesh.id = desc->linked_id;
            break;
        case SPSceneNodeType_Light:
            node->linked_object.light.id = desc->linked_id;
            break;
        default:
            node->linked_object.type = SPSceneNodeType_Empty; // to prevent Force32
            node->linked_object.empty.id = desc->linked_id;
            break;
    }
    spSceneNodeMarkDirty(node);
    return node_id;
}

void _spSceneNodeUpdateWorldTransform(SPSceneNode* node) {
    SP_ASSERT(node);
    glm_mat4_identity(node->_transform_world);
    glm_translate(node->_transform_world, node->transform.pos);
    mat4 scale = GLM_MAT4_IDENTITY_INIT;
    glm_scale(scale, node->transform.scale);
    vec3 rot_rad = {
        glm_rad(node->transform.rot[0]),
        glm_rad(node->transform.rot[1]),
        glm_rad(node->transform.rot[2])
    };
    mat4 rot = GLM_MAT4_IDENTITY_INIT;
    glm_euler_zxy(rot_rad, rot);
    glm_mat4_mul(rot, scale, rot);
    glm_mat4_mul(node->_transform_world, rot, node->_transform_world);
    // assuming parent world transform is already updated
    if(node->tree.parent) {
        glm_mat4_mul(
            node->tree.parent->_transform_world,
            node->_transform_world,
            node->_transform_world
        );
    }
    switch (node->tree.children.count) {
        case 0:
            break;
        case 1:
            _spSceneNodeUpdateWorldTransform(node->tree.children.single);
            break;
        default:
            for(uint32_t i = 0; i < node->tree.children.count; i++) {
                _spSceneNodeUpdateWorldTransform(node->tree.children.list[i]);
            }
            break;
    }

}