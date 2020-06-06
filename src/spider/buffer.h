#ifndef SPIDER_BUFFER_H_
#define SPIDER_BUFFER_H_

#include <stdint.h>

#include <webgpu/webgpu.h>

typedef struct _SPGpuBuffer {
    WGPUBuffer buffer;
    WGPUBufferUsage usage;
    uint32_t size;
} _SPGpuBuffer;

typedef struct _SPGpuBufferDesc {
    const char* label;
    WGPUBufferUsage usage;
    uint32_t size;
    struct {
        const void* data;
        uint32_t size;
    } initial;
} _SPGpuBufferDesc;

/*
Creates a _SPGpuBuffer object
*/
_SPGpuBuffer _spCreateGpuBuffer(const _SPGpuBufferDesc* desc);

/*
Copies data into dest.buffer via a temporary staging buffer
Doesn't submit the resulting command
*/
void _spRecordCopyDataToBuffer(WGPUCommandEncoder encoder, _SPGpuBuffer dest, uint32_t dest_offset, const void* data, uint32_t size);



#endif // SPIDER_BUFFER_H_