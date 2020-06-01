#include "file.h"

#include <stdio.h>

#include "debug.h"
#include "impl.h"

void _spReadFile(const char* filename, _SPFileReadResult* result) {
    SPIDER_ASSERT(filename && result);
    FILE *f = fopen(filename, "rb");
    if(f == NULL) {
        DEBUG_PRINT(DEBUG_PRINT_WARNING, "couldn't open file '%s'\n", filename);
    }
    fseek(f, 0, SEEK_END);
    result->size = ftell(f);
    fseek(f, 0, SEEK_SET);

    result->data = SPIDER_MALLOC(result->size);
    fread(result->data, 1, result->size, f);
    fclose(f);
}