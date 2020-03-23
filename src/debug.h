#ifndef DEBUG_H_
#define DEBUG_H_

#define DEBUG_PRINT_TYPE_INIT 1
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 1
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 1
#define DEBUG_PRINT_RENDER 0
#define DEBUG_PRINT_METRICS 1

#include <stdio.h>
#define DEBUG_PRINT(should_print, ...) do{if(should_print) printf(__VA_ARGS__);}while(0)

#define DEBUG_PRINT_MAT4(should_print, name, mat) \
do{ \
    if(should_print){\
        printf("%s:\n", name); \
        printf("%.2f %.2f %.2f %.2f\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]); \
        printf("%.2f %.2f %.2f %.2f\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]); \
        printf("%.2f %.2f %.2f %.2f\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]); \
        printf("%.2f %.2f %.2f %.2f\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]); \
    } \
} while(0)

#endif // DEBUG_H_