#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <webgpu/webgpu.h>
#include <emscripten.h>
#include <emscripten/html5.h>

const uint32_t vsCode[] = {
    119734787, 65536, 851975, 38, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 458767, 0, 4, 1852399981, 0, 10, 25, 196611, 1, 310, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425, 1701734764, 1919509599, 1769235301, 25974, 524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449, 262149, 4, 1852399981, 0, 393221, 8, 1348430951, 1700164197, 2019914866, 0, 393222, 8, 0, 1348430951, 1953067887, 7237481, 458758, 8, 1, 1348430951, 1953393007, 1702521171, 0, 196613, 10, 0, 393221, 25, 1449094247, 1702130277, 1684949368, 30821, 327685, 28, 1701080681, 1818386808, 101, 327752, 8, 0, 11, 0, 327752, 8, 1, 11, 1, 196679, 8, 2, 262215, 25, 11, 42, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262174, 8, 7, 6, 262176, 9, 3, 8, 262203, 9, 10, 3, 262165, 11, 32, 1, 262187, 11, 12, 0, 262167, 13, 6, 2, 262165, 14, 32, 0, 262187, 14, 15, 3, 262172, 16, 13, 15, 262187, 6, 17, 0, 262187, 6, 18, 1056964608, 327724, 13, 19, 17, 18, 262187, 6, 20, 3204448256, 327724, 13, 21, 20, 20, 327724, 13, 22, 18, 20, 393260, 16, 23, 19, 21, 22, 262176, 24, 1, 11, 262203, 24, 25, 1, 262176, 27, 7, 16, 262176, 29, 7, 13, 262187, 6, 32, 1065353216, 262176, 36, 3, 7, 327734, 2, 4, 0, 3, 131320, 5, 262203, 27, 28, 7, 262205, 11, 26, 25, 196670, 28, 23, 327745, 29, 30, 28, 26, 262205, 13, 31, 30, 327761, 6, 33, 31, 0, 327761, 6, 34, 31, 1, 458832, 7, 35, 33, 34, 17, 32, 327745, 36, 37, 10, 12, 196670, 37, 35, 65789, 65592
};
const uint32_t fsCode[] = {
    119734787, 65536, 851975, 14, 0, 131089, 1, 393227, 1, 1280527431, 1685353262, 808793134, 0, 196622, 0, 1, 393231, 4, 4, 1852399981, 0, 9, 196624, 4, 7, 196611, 1, 310, 655364, 1197427783, 1279741775, 1885560645, 1953718128, 1600482425, 1701734764, 1919509599, 1769235301, 25974, 524292, 1197427783, 1279741775, 1852399429, 1685417059, 1768185701, 1952671090, 6649449, 262149, 4, 1852399981, 0, 327685, 9, 1734439526, 1869377347, 114, 196679, 9, 0, 262215, 9, 30, 0, 131091, 2, 196641, 3, 2, 196630, 6, 32, 262167, 7, 6, 4, 262176, 8, 3, 7, 262203, 8, 9, 3, 262187, 6, 10, 0, 262187, 6, 11, 1056964608, 262187, 6, 12, 1065353216, 458796, 7, 13, 10, 11, 12, 12, 327734, 2, 4, 0, 3, 131320, 5, 196670, 9, 13, 65789, 65592
};

static WGPUDevice device;
static WGPUQueue queue;
static WGPUBuffer readbackBuffer;
static WGPURenderPipeline pipeline;
static WGPUSwapChain swapChain;
static bool done = false;

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

void errorCallback(WGPUErrorType type, char const * message, void * userdata) {
    printf("%d: %s\n", type, message);
}

FileReadResult vertShader = { .size = 0, .data = NULL };
FileReadResult fragShader = { .size = 0, .data = NULL };

void init() {
    printf("init: start\n");
    device = emscripten_webgpu_get_device();
    printf("init: got device\n");
    wgpuDeviceSetUncapturedErrorCallback(device, &errorCallback, NULL);
    
    queue = wgpuDeviceCreateQueue(device);
    printf("init: created queue\n");

    readFile("src/shaders/vert.spv", &vertShader);
    printf("read file: size: %d\n", vertShader.size);
    WGPUShaderModule vsModule;
    {
        WGPUShaderModuleDescriptor descriptor = {
            .codeSize = vertShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)vertShader.data
        };
        vsModule = wgpuDeviceCreateShaderModule(device, &descriptor);
    }
    printf("init: created vsModule\n");

    readFile("src/shaders/frag.spv", &fragShader);
    printf("read file: size: %d\n", fragShader.size);
    WGPUShaderModule fsModule;
    {
        WGPUShaderModuleDescriptor descriptor = {
            .codeSize = fragShader.size / sizeof(uint32_t),
            .code = (const uint32_t*)fragShader.data
        };
        fsModule = wgpuDeviceCreateShaderModule(device, &descriptor);
    }
    printf("init: created fsModule\n");

    WGPUBindGroup bindGroup;
    {
        WGPUBindGroupLayoutDescriptor bglDesc;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
        WGPUBindGroupDescriptor desc = {
            .layout = bgl,
            .bindingCount = 0,
            .bindings = NULL
        };
        bindGroup = wgpuDeviceCreateBindGroup(device, &desc);
    }
    printf("init: created bindGroup\n");

    {
        WGPUProgrammableStageDescriptor fragmentStage = {
            .module = fsModule,
            .entryPoint = "main"
        };
        printf("init: created fragmentStage\n");

        WGPUColorStateDescriptor colorStateDescriptor = {
            .format = WGPUTextureFormat_BGRA8Unorm,
            .alphaBlend = {
                .operation = WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor_One,
                .dstFactor = WGPUBlendFactor_Zero,
            },
            .colorBlend = {
                .operation = WGPUBlendOperation_Add,
                .srcFactor = WGPUBlendFactor_One,
                .dstFactor = WGPUBlendFactor_Zero,
            },
            .writeMask = WGPUColorWriteMask_All
        };
        printf("init: created colorStateDescriptor\n");

        WGPUPipelineLayoutDescriptor pl = {
            .bindGroupLayoutCount = 0,
            .bindGroupLayouts = NULL
        };
        printf("init: created pipelineLayoutDescriptor\n");

        WGPURenderPipelineDescriptor pipelineDesc = {
            .layout = wgpuDeviceCreatePipelineLayout(device, &pl),
            .vertexStage.module = vsModule,
            .vertexStage.entryPoint = "main",
            .fragmentStage = &fragmentStage,
            .colorStateCount = 1,
            .colorStates = &colorStateDescriptor,
            .primitiveTopology = WGPUPrimitiveTopology_TriangleList,
            .sampleCount = 1,
            .sampleMask = 0xFFFFFFFF
        };
        pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        printf("init: created pipeline\n");
    }
}

void shutdown() {
    if(vertShader.size > 0) {
        free(vertShader.data);
    }
    if(fragShader.size > 0) {
        free(fragShader.data);
    }
}

void render(WGPUTextureView view) {
    WGPURenderPassColorAttachmentDescriptor attachment = {
        .attachment = view,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearColor = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f}
    };
    //printf("render: created renderPassColorAttachmentDescriptor\n");

    WGPURenderPassDescriptor renderpass = {
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment
    };
    //printf("render: created renderPassDescriptor\n");

    WGPUCommandBuffer commands;
    {
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);
        {
            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderpass);
            wgpuRenderPassEncoderSetPipeline(pass, pipeline);
            wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
            wgpuRenderPassEncoderEndPass(pass);
            //printf("render: finished renderPass\n");
        }
        commands = wgpuCommandEncoderFinish(encoder, NULL);
        //printf("render: finished commandEncoder\n");
    }

    wgpuQueueSubmit(queue, 1, &commands);
    //printf("render: submitted queue\n");
}

void frame() {
    WGPUTextureView backbuffer = wgpuSwapChainGetCurrentTextureView(swapChain);
    render(backbuffer);
}

int main(void) {
    init();
    {
        WGPUSurfaceDescriptorFromHTMLCanvasId canvasDesc = {
            .nextInChain = NULL,
            .sType = WGPUSType_SurfaceDescriptorFromHTMLCanvasId,
            .id = "canvas"
        };

        WGPUSurfaceDescriptor surfDesc = {
            .nextInChain = (const WGPUChainedStruct*)&canvasDesc
        };
        WGPUInstance instance;  // null instance
        WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfDesc);
        printf("main: created surface\n");

        WGPUSwapChainDescriptor scDesc = {
            .usage = WGPUTextureUsage_OutputAttachment,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .width = 640,
            .height = 480,
            .presentMode = WGPUPresentMode_VSync
        };
        swapChain = wgpuDeviceCreateSwapChain(device, surface, &scDesc);
        printf("main: created swapChain\n");
    }
    emscripten_set_main_loop(frame, 30, false);
    shutdown();
    return 0;
}