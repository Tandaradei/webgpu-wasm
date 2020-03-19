#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <webgpu/webgpu.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#include "vertex.h"

#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])

const Vertex vertices[] = {
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}}, // TL
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // TR
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // BR
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}}, // BL
};

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0
};

UniformBufferObject ubo = {
    .model_view = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    },
    .proj = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    }
};

static WGPUBuffer vertexBuffer;
static WGPUBuffer indexBuffer;
static WGPUBuffer uniformStagingBuffer;
static WGPUBuffer uniformBuffer;

static WGPUDevice device;
static WGPUQueue queue;
static WGPUBuffer readbackBuffer;
static WGPURenderPipeline pipeline;
static WGPUSwapChain swapChain;

static WGPUBindGroup bindGroup;
static WGPUBindGroupLayout bgl;

static bool done = false;
clock_t startClock;

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
    startClock = clock();
    printf("init: start\n");
    device = emscripten_webgpu_get_device();
    printf("init: got device\n");
    wgpuDeviceSetUncapturedErrorCallback(device, &errorCallback, NULL);
    
    queue = wgpuDeviceCreateQueue(device);
    printf("init: created queue\n");

    readFile("src/shaders/compiled/vert.spv", &vertShader);
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

    readFile("src/shaders/compiled/frag.spv", &fragShader);
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

    // Uniform buffer creation
    {
        WGPUBufferDescriptor bufferDesc = {
            .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
            .size = sizeof(UniformBufferObject)
        };

        uniformBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

        /*
        memcpy(result.data, &ubo, result.dataLength);
        uniformBuffer = result.buffer;
        wgpuBufferUnmap(uniformBuffer);
        */
    }

    // Uniform buffer creation
    {
        WGPUBufferDescriptor bufferDesc = {
            .usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc,
            .size = sizeof(UniformBufferObject)
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(device, &bufferDesc);

        memcpy(result.data, &ubo, result.dataLength);
        uniformStagingBuffer = result.buffer;
        wgpuBufferUnmap(uniformStagingBuffer);
    }

    {
        WGPUBindGroupLayoutBinding bindingLayouts[] = {
            {
                .binding = 0,
                .visibility = WGPUShaderStage_Vertex,
                .type = WGPUBindingType_UniformBuffer,
                .hasDynamicOffset = false,
                .multisampled = false,
                .textureDimension = WGPUTextureViewDimension_Undefined,
                .textureComponentType = WGPUTextureComponentType_Float,
            }
        };

        WGPUBindGroupLayoutDescriptor bglDesc = {
            .bindingCount = ARRAY_LEN(bindingLayouts),
            .bindings = bindingLayouts
        };
        bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
        
        WGPUBindGroupBinding bindings[] = {
            {
                .binding = 0,
                .buffer = uniformBuffer,
                .offset = 0,
                .size = sizeof(UniformBufferObject),
                .sampler = NULL,
                .textureView = NULL,
            }
        };

        WGPUBindGroupDescriptor desc = {
            .layout = bgl,
            .bindingCount = ARRAY_LEN(bindings),
            .bindings = bindings
        };
        bindGroup = wgpuDeviceCreateBindGroup(device, &desc);
    }
    printf("init: created bindGroup\n");

    // Render pipeline creation
    {
        WGPUPipelineLayoutDescriptor pl = {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &bgl
        };
        printf("init: created pipelineLayoutDescriptor\n");

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

        WGPUVertexAttributeDescriptor vertexAttributeDescs[] = {
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(Vertex, pos),
                .shaderLocation = 0
            },
            {
                .format = WGPUVertexFormat_Float3,
                .offset = offsetof(Vertex, color),
                .shaderLocation = 1
            }
        };

        WGPUVertexBufferLayoutDescriptor vertexBufferLayoutDescs[] = {
            {
                .arrayStride = sizeof(Vertex),
                .stepMode = WGPUInputStepMode_Vertex,
                .attributeCount = ARRAY_LEN(vertexAttributeDescs),
                .attributes = vertexAttributeDescs
            }
        };

        WGPUVertexStateDescriptor vertexStateDesc = {
            .indexFormat = WGPUIndexFormat_Uint16,
            .vertexBufferCount = ARRAY_LEN(vertexBufferLayoutDescs),
            .vertexBuffers = vertexBufferLayoutDescs
        };

        WGPURasterizationStateDescriptor rastDesc = {
            .nextInChain = NULL,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None,
            .depthBias = 0,
            .depthBiasSlopeScale = 0.0f,
            .depthBiasClamp = 0.0f,
        };

        WGPURenderPipelineDescriptor pipelineDesc = {
            .layout = wgpuDeviceCreatePipelineLayout(device, &pl),
            .vertexStage.module = vsModule,
            .vertexStage.entryPoint = "main",
            .vertexState = &vertexStateDesc,
            .fragmentStage = &fragmentStage,
            .rasterizationState = &rastDesc,
            .colorStateCount = 1,
            .colorStates = &colorStateDescriptor,
            .primitiveTopology = WGPUPrimitiveTopology_TriangleList,
            .sampleCount = 1,
            .sampleMask = 0xFFFFFFFF
        };
        pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        printf("init: created pipeline\n");
    }

    // Vertex buffer creation
    {
        WGPUBufferDescriptor bufferDesc = {
            .usage = WGPUBufferUsage_Vertex,
            .size = sizeof(vertices)
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(device, &bufferDesc);

        memcpy(result.data, vertices, result.dataLength);
        vertexBuffer = result.buffer;
        wgpuBufferUnmap(vertexBuffer);
    }

    // Index buffer creation
    {
        WGPUBufferDescriptor bufferDesc = {
            .usage = WGPUBufferUsage_Index,
            .size = sizeof(indices)
        };

        WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(device, &bufferDesc);

        memcpy(result.data, indices, result.dataLength);
        indexBuffer = result.buffer;
        wgpuBufferUnmap(indexBuffer);
    }
}

void shutdown() {
    printf("shutdown: start\n");
    if(vertShader.size > 0) {
        free(vertShader.data);
    }
    if(fragShader.size > 0) {
        free(fragShader.data);
    }
    wgpuBufferDestroy(vertexBuffer);
    wgpuBufferDestroy(indexBuffer);
}

void writeUBOCallback(WGPUBufferMapAsyncStatus status, void * data, uint64_t dataLength, void * userdata) {
    printf("entered callback\n");
    memcpy(data, userdata, dataLength);
    wgpuBufferUnmap(uniformStagingBuffer);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, NULL);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, uniformStagingBuffer, 0, uniformBuffer, 0, sizeof(UniformBufferObject));
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);

    wgpuQueueSubmit(queue, 1, &commands);
}

void update() {
    clock_t curClock = clock();
    float diff = ((float)(curClock - startClock) / CLOCKS_PER_SEC);

    vec3 axis = {
        0.0f, 0.0, 1.0f
    };
    mat4 identity = GLM_MAT4_IDENTITY_INIT;
    glm_rotate(identity, diff * glm_rad(90.0f), axis);
    mat4 model = GLM_MAT4_IDENTITY_INIT;
    memcpy(model, identity, sizeof(mat4));
    vec3 eye = {
        2.0f, 2.0f, 2.0f
    };
    vec3 center = {
        0.0f, 0.0f, 0.0f
    };
    vec3 up = {
        0.0f, 0.0f, 1.0f
    };
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    glm_lookat(eye, center, up, view);
    glm_mat4_mul(view, model, ubo.model_view);
    glm_perspective(glm_rad(45.0f), 640.0f / 480.0f, 0.1f, 10.0f, ubo.proj);
    wgpuBufferMapWriteAsync(uniformStagingBuffer, writeUBOCallback, &ubo);
    printf("update: end\n");
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
            wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, NULL);
            wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0);
            wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, 0);
            wgpuRenderPassEncoderDrawIndexed(pass, ARRAY_LEN(indices), 1, 0, 0, 0);
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
    update();
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
    //shutdown();
    return 0;
}