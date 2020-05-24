#ifndef SPIDER_FILE_H_
#define SPIDER_FILE_H_

#include <stdint.h>

typedef struct _SPFileReadResult {
    uint32_t size;
    char* data;
} _SPFileReadResult;

void _spReadFile(const char* filename, _SPFileReadResult* result);

#endif // SPIDER_FILE_H_