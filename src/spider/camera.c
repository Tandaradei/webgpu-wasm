#include "camera.h"

#include <string.h>

#include <cglm/cglm.h>

void _spPerspectiveMatrixReversedZ(float fovy, float aspect, float near, float far, mat4 dest) {
    glm_mat4_zero(dest);
    float f = 1.0f / tanf(fovy * 0.5f);
    float range = far / (near - far);
    memcpy(dest, &(mat4){
        {f / aspect, 0.0f, 0.0f, 0.0f}, // first COLUMN
        {0.0f, f, 0.0f, 0.0f}, // second COLUMN
        {0.0f, 0.0f, -range - 1, -1.0f}, // third COLUMN
        {0.0f, 0.0f, -near * range, 0.0f} // fourth COLUMN
    }, sizeof(mat4));
}

void _spPerspectiveMatrixReversedZInfiniteFar(float fovy, float aspect, float near, mat4 dest) {
    glm_mat4_zero(dest);
    float f = 1.0f / tanf(fovy * 0.5f);
    memcpy(dest, &(mat4){
        {f / aspect, 0.0f, 0.0f, 0.0f}, // first COLUMN
        {0.0f, f, 0.0f, 0.0f}, // second COLUMN
        {0.0f, 0.0f, 0.0f, -1.0f}, // third COLUMN
        {0.0f, 0.0f, near, 0.0f} // fourth COLUMN
    }, sizeof(mat4));
}
