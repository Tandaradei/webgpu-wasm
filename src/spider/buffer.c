#include "buffer.h"

#include "impl.h"
#include "state.h"

extern _SPState _sp_state;

_SPGpuBuffer _spCreateGpuBuffer(const _SPGpuBufferDesc* desc) {
    _SPGpuBuffer buffer = {
        .usage = desc->usage,
        .size = desc->size,
    };

    WGPUBufferDescriptor buffer_desc = {
        .label = desc->label,
        .usage = desc->usage,
        .size = desc->size,
    };
    if(desc->initial.data && desc->initial.size > 0 && desc->initial.size <= desc->size) {
        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
        SP_ASSERT(result.data && result.buffer);
        memcpy(result.data, desc->initial.data, desc->initial.size);
        wgpuBufferUnmap(result.buffer);
        buffer.buffer = result.buffer;
    }
    else {
        buffer.buffer = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
    }
    return buffer;
}

void _spRecordCopyDataToBuffer(WGPUCommandEncoder encoder, _SPGpuBuffer dest, uint32_t dest_offset, const void* data, uint32_t size) {
    SP_ASSERT(encoder);
    SP_ASSERT(data && size > 0);
    SP_ASSERT(dest.buffer && dest.size >= dest_offset + size && (dest.usage & WGPUBufferUsage_CopyDst));
    WGPUBufferDescriptor staging_buffer_desc = {
        .usage = WGPUBufferUsage_CopySrc,
        .size = size,
    };

    WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &staging_buffer_desc);
    SP_ASSERT(result.data && result.buffer);
    memcpy(result.data, data, size);
    wgpuBufferUnmap(result.buffer);

    wgpuCommandEncoderCopyBufferToBuffer(
        encoder,
        result.buffer, 0,
        dest.buffer, dest_offset,
        size
    );
    _SP_RELEASE_RESOURCE(Buffer, result.buffer);
}