#include "instance.h"

#include "debug.h"
#include "impl.h"
#include "state.h"

extern _SPState _sp_state;

SPInstanceID spCreateInstance(const SPInstanceDesc* desc) {
    SPIDER_ASSERT(desc->object.mesh.id != SP_INVALID_ID && desc->object.material.id != SP_INVALID_ID);
    if(!desc->object.mesh.id || !desc->object.material.id) {
        return (SPInstanceID){SP_INVALID_ID};
    }
    SPInstanceID instance_id = (SPInstanceID){_spAllocPoolIndex(&(_sp_state.pools.instance_pool))};
    if(instance_id.id == SP_INVALID_ID) {
        return instance_id;
    }
    int id = instance_id.id; 
    SPInstance* instance = &(_sp_state.pools.instances[id]);
    instance->object.mesh = desc->object.mesh;
    instance->object.material = desc->object.material;
    if(desc->transform) {
        memcpy(&(instance->transform), desc->transform, sizeof(SPTransform));
    }
    else {
        instance->transform = (SPTransform){
            .scale = {1.0f, 1.0f, 1.0f},
        };
    }
    return instance_id;
}
