#ifndef SPIDER_imgui_impl_spider_H_
#define SPIDER_imgui_impl_spider_H_

#define _SP_IMGUI_MAX_VERTICES_DEFAULT 65536
#define _SP_IMGUI_MAX_CMD_LISTS_DEFAULT 1024

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include "render_pipeline.h"
#include "buffer.h"

typedef struct SPInitImGuiDesc {
    uint32_t max_vertices;
    uint32_t max_cmd_lists;

    // TODO: this should be dynamic
    struct {
        uint32_t width;
        uint32_t height;
    } display_size;

    WGPUTextureFormat color_format;

} SPInitImGuiDesc;

typedef struct _SPImGuiState {
    uint32_t max_vertices;
    uint32_t max_cmd_lists;

    _SPGpuBuffer vertex_buffer;
    _SPGpuBuffer index_buffer;
    WGPUSampler font_sampler;
    WGPUTexture font_texture;
    WGPUTextureView font_texture_view;
    _SPRenderPipeline pipeline;

    // TODO: this should be dynamic
    // ---
    struct {
        uint32_t width;
        uint32_t height;
    } display_size;
    _SPGpuBuffer uniform_buffer;
    struct {
        WGPUBindGroup uniform;
        WGPUBindGroup vert;
        WGPUBindGroup frag;
    } bind_groups;
    // ---

    struct {
        struct {
            ImDrawVert* data;
            uint32_t* offsets;
        } vertex;
        struct {
            ImDrawIdx* data;
            uint32_t* offsets;
        } index;
    } update;


} _SPImGuiState;

void _spImGuiInit(const SPInitImGuiDesc* desc);
void _spImGuiShutdown(_SPImGuiState* imgui);

void _spImGuiNewFrame(uint32_t width, uint32_t height, float delta_time);
void _spImGuiRender(WGPUTextureView view);


#endif // SPIDER_imgui_impl_spider_H_