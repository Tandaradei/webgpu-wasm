#ifndef SPIDER_DEBUG_H_
#define SPIDER_DEBUG_H_

#ifdef DEBUG
#define DEBUG_PRINT_ALLOWED 1
#else
#define DEBUG_PRINT_ALLOWED 0
#endif
#define DEBUG_PRINT_TYPE_INIT 0
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 0
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 0
#define DEBUG_PRINT_MESH 0
#define DEBUG_PRINT_UPDATE 1
#define DEBUG_PRINT_RENDER 0
#define DEBUG_PRINT_METRICS 0
#define DEBUG_PRINT_GLTF_LOAD 1
#define DEBUG_PRINT_SHADER_LOAD 0
#define DEBUG_PRINT_IMGUI 1
#define DEBUG_PRINT_RENDER_PIPELINE 1
#define DEBUG_PRINT_INPUT 1

#include "state.h"
extern _SPState _sp_state;

#define DEBUG_PRINT(should_print, ...) do{if(DEBUG_PRINT_ALLOWED && should_print){ printf("[%u] ", _sp_state.frame_index);printf(__VA_ARGS__); }}while(0)

#define DEBUG_PRINT_MAT4(should_print, name, mat) \
do{ \
    if(DEBUG_PRINT_ALLOWED && should_print){\
        printf("[%u] %s:\n", _sp_state.frame_index, name); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]); \
        printf("     %.2f %.2f %.2f %.2f\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]); \
    } \
} while(0)

#endif // SPIDER_DEBUG_H_