#include "render_mesh.h"

#include "impl.h"
#include "state.h"

extern _SPState _sp_state;

SPRenderMeshID spCreateRenderMesh(const SPRenderMeshDesc* desc) {
    SPRenderMeshID rm_id = (SPRenderMeshID){_spAllocPoolIndex(&(_sp_state.pools.render_mesh.info))};
    if(rm_id.id == SP_INVALID_ID) {
        return rm_id;
    }
    int id = rm_id.id; 
    SPRenderMesh* rm = &(_sp_state.pools.render_mesh.data[id]);
    rm->mesh_id = desc->mesh;
    rm->material_id = desc->material;
    rm->_mesh = spGetMesh(desc->mesh);
    rm->_material = spGetMaterial(desc->material);
    return rm_id;
}