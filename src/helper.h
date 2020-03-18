#ifndef HELPER_H_
#define HELPER_H_

#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])


#define SHORT_STR_SIZE 16
typedef struct short_str {
    char buf[SHORT_STR_SIZE];
 } short_str;

typedef struct FileReadResult {
    uint32_t size;
    char* data;
} FileReadResult;

void readFile(const char* filename, FileReadResult* result) {
    FILE *f = fopen(filename, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    result->size = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    result->data = malloc(result->size);
    fread(result->data, 1, result->size, f);
    fclose(f);
}

#endif // HELPER_H_