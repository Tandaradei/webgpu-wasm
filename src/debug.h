#ifndef DEBUG_H_
#define DEBUG_H_

#define DEBUG_PRINT_TYPE_INIT 1
#define DEBUG_PRINT_TYPE_CREATE_MATERIAL 1
#define DEBUG_PRINT_WARNING 1
#define DEBUG_PRINT_GENERAL 1

#include <stdio.h>
#define DEBUG_PRINT(should_print, ...) do{if(should_print) printf(__VA_ARGS__);}while(0);

#endif // DEBUG_H_