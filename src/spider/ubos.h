#ifndef SPIDER_UBOS_H_
#define SPIDER_UBOS_H_

#include <webgpu/webgpu.h>
#include <cglm/cglm.h>

typedef struct _SPUboCamera {
    mat4 view;
    mat4 proj;
    vec3 pos;
} _SPUboCamera;

typedef struct _SPUboModel {
    mat4 model;
} _SPUboModel;

typedef struct _SPUboLight {
    mat4 view; // 64 - 4 blocks
    mat4 proj; // 64 - 4 blocks
    vec4 pos3_range1; // 16
    vec4 color3_type1; // 16
    vec4 dir3_fov1; // for spot & dir - 16
    vec4 area2_power1_padding1; // for dir - 16
} _SPUboLight; // 192 bytes

/* 
Copies the world transform matrix of each render mesh via a staging buffer 
to the 'model' buffer 
*/
void _spUpdateUboModel(void);

/* Copies the view and projection matrices to via a staging buffer 
to the 'camera' buffer  */
void _spUpdateUboCamera(void);

/* Copies the light properties for each light (currently just one) 
via a staging buffer to the 'light' buffer  */
void _spUpdateUboLight(void);

#endif // SPIDER_UBOS_H_