#ifndef SPIDER_CAMERA_H_
#define SPIDER_CAMERA_H_

#include <cglm/cglm.h>

typedef enum SPCameraMode {
    SPCameraMode_Direction,
    SPCameraMode_LookAt
} SPCameraMode;

typedef struct SPCamera {
    vec3 pos;
    vec3 dir;
    vec3 look_at;
    SPCameraMode mode;
    float fovy;
    float aspect;
    float near;
    float far;  // not used with infinite far plane
    mat4 _view;
    mat4 _proj;
} SPCamera;


/*
Creates a perspective matrix [near = 1, far = 0] 
http://dev.theomader.com/depth-precision/
*/
void _spPerspectiveMatrixReversedZ(float fovy, float aspect, float near, float far, mat4 dest);

/*
Creates a perspective matrix [near = 1, infinite = 0] without far plane
http://dev.theomader.com/depth-precision/
*/
void _spPerspectiveMatrixReversedZInfiniteFar(float fovy, float aspect, float near, mat4 dest);


#endif // SPIDER_CAMERA_H_