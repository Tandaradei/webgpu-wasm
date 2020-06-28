async function loadShader(shaderPath) {
   return await fetch(new Request(shaderPath), { method: 'GET', mode: 'cors' }).then((res) =>
        res.arrayBuffer().then((arr) => new Uint32Array(arr))
    );
}

function initBuffer(data, usage, device, queue) {
    const use_write_buffer = true;
    if(use_write_buffer && queue["writeBuffer"] !== undefined){
        var buffer = device.createBuffer({
            size: data.byteLength,
            usage: usage | GPUBufferUsage.COPY_DST
        });
        queue.writeBuffer(buffer, 0, data.buffer);
        return buffer;
    }
    else {
        var [buffer, buffer_data] = device.createBufferMapped({
            size: data.byteLength,
            usage: usage
        });
        var writeArray = data instanceof Uint16Array ? new Uint16Array(buffer_data) : new Float32Array(buffer_data);
        writeArray.set(data);
        buffer.unmap();
        return buffer;
    }
}

async function start() {
    var gpu = navigator.gpu;
    var adapter = await gpu.requestAdapter({powerPreference: "high-performance"});
    var device = await adapter.requestDevice();
    var queue = device.defaultQueue;
    // Vertex Data (4 vertices with each 3 float for position and 3 float for color)
    const vertex_data = new Float32Array([
    //     x,    y,   z,      r,   g,   b
        -1.0,  1.0, 0.5,    1.0, 0.0, 0.0,
         1.0,  1.0, 0.5,    0.0, 1.0, 0.0,
        -1.0, -1.0, 0.5,    0.0, 0.0, 1.0,
         1.0, -1.0, 0.5,    1.0, 1.0, 1.0,
    ]);

    var vertex_buffer = initBuffer(vertex_data, GPUBufferUsage.VERTEX, device, queue);

    // Index data (2 triangles with each 3 indices)
    const index_data = new Uint16Array([
        0, 1, 2, 
        2, 1, 3
    ]);

    var index_buffer = initBuffer(index_data, GPUBufferUsage.INDEX, device, queue);

    var texture = device.createTexture({
        size: {
            width: 1024,
            height: 1024,
            depth: 1
        },
        mipLevelCount: 1,
        sampleCount: 1,
        dimension: '2d',
        format: 'rgba8unorm',
        usage: GPUTextureUsage.SAMPLED | GPUTextureUsage.COPY_DST
    });
    console.log(texture);

    var texture_data = new Uint32Array(1024 * 1024);
    texture_data.fill(0x00FF00FF); // 0 red, 255 blue, 0 green, 255 alpha

    var texture_copy_view = {
        texture: texture,
        mipLevel: 0,
        origin: {
            x: 0,
            y: 0,
            z: 0
        }
    };
    
    var data_layout = {
        offset: 0,
        bytesPerRow: 1024 * 4,
        rowsPerImage: 1024
    };

    var copy_size = {
        width: 0,
        height: 0,
        depth: 0
    };
    if(queue["writeTexture"] !== undefined) {
        queue.writeTexture(texture_copy_view, texture_data, data_layout, copy_size);
    }
    else {
        console.log("writeTexture not implemented");
    }

    // bgle = BindGroupLayoutEntry
    // bgl = BindGroupLayout

    var bgle_view_proj = {
        binding: 0,
        visibility: GPUShaderStage.VERTEX,
        type: 'uniform-buffer',
        hasDynamicOffset: false
    };
    
    var bgl_common = device.createBindGroupLayout({
        entries: [
            bgle_view_proj
        ]
    });

    var bgle_model = {
        binding: 0,
        visibility: GPUShaderStage.VERTEX,
        type: 'uniform-buffer',
        hasDynamicOffset: false
    };

    var bgl_dynamic = device.createBindGroupLayout({
        entries: [
            bgle_model
        ]
    });

    var pipeline_layout = device.createPipelineLayout({
        bindGroupLayouts: [
            bgl_common,
            bgl_dynamic
        ]
    });

    var canvas = document.getElementById('canvas');
    var canvas_context = canvas.getContext('gpupresent');
    const canvas_format = await canvas_context.getSwapChainPreferredFormat(device);
    var swap_chain = canvas_context.configureSwapChain({
        device: device,
        format: canvas_format,
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT
    });

    const vertex_shader_code_spirv = await loadShader('simple.vert.spv');
    const frag_shader_code_spirv = await loadShader('simple.frag.spv');

    var render_pipeline = device.createRenderPipeline({
        layout: pipeline_layout,
        vertexState: {
            indexFormat: 'uint16',
            vertexBuffers: [
                {
                    arrayStride: 24, // 24 bytes per vertex
                    stepMode: 'vertex',
                    attributes: [
                        { // pos attribute
                            format: 'float3',
                            offset: 0,
                            shaderLocation: 0
                        },
                        { // color attribute
                            format: 'float3',
                            offset: 12, // offset in bytes
                            shaderLocation: 1
                        }
                    ]
                }
            ]
        },
        vertexStage: {
            module: device.createShaderModule({
                code: vertex_shader_code_spirv
            }),
            entryPoint: 'main'
        },
        primitiveTopology: 'triangle-list',
        rasterizationState: {
            frontFace: 'cw', // clock wise
            cullMode: 'none', // don't cull faces
        },
        fragmentStage: {
            module: device.createShaderModule({
                code: frag_shader_code_spirv
            }),
            entryPoint: 'main'
        },
        depthStencilState: {
            format: 'depth32float',
            depthWriteEnabled: true,
            depthCompare: 'greater'
        },
        colorStates: [
            {
                format: canvas_format,
                alphaBlend: { 
                    srcFactor: 'one', 
                    dstFactor: 'zero', 
                    operation: 'add'
                },
                colorBlend: { 
                    srcFactor: 'one', 
                    dstFactor: 'zero', 
                    operation: 'add'
                },
                writeMask: GPUColorWrite.ALL
            }
        ]
    });

    var common_data = new Float32Array([
        // view matrix
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
        // proj matrix
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]);

    var common_buffer = initBuffer(common_data, GPUBufferUsage.UNIFORM, device, queue);

    var bind_group_common = device.createBindGroup({
        layout: bgl_common,
        entries: [
            {
                binding: 0,
                resource: {
                    buffer: common_buffer,
                    offset: 0,
                    size: common_data.byteLength
                }
            }
        ]
    });

    var model_data = new Float32Array([
        // model matrix (translate 2 units in Z direction)
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    ]);

    var dynamic_buffer = initBuffer(model_data, GPUBufferUsage.UNIFORM, device, queue);

    var bind_group_dynamic = device.createBindGroup({
        layout: bgl_dynamic,
        entries: [
            {
                binding: 0,
                resource: {
                    buffer: dynamic_buffer,
                    offset: 0,
                    size: model_data.byteLength
                }
            }
        ]
    });
    
    render(device, swap_chain, queue, canvas, render_pipeline, vertex_buffer, index_buffer, bind_group_common, bind_group_dynamic, index_data);

}

function render(device, swap_chain, queue, canvas, render_pipeline, vertex_buffer, index_buffer, bind_group_common, bind_group_dynamic, index_data) {
    var color_texture = swap_chain.getCurrentTexture();
    var depth_texture = device.createTexture({
        size: {
            width: canvas.width,
            height: canvas.height,
            depth: 1
        },
        mipLevelCount: 1,
        sampleCount: 1,
        dimension: '2d',
        format: 'depth32float',
        usage: GPUTextureUsage.OUTPUT_ATTACHMENT
    });

    var command_enc = device.createCommandEncoder();
    var render_pass = command_enc.beginRenderPass({
        colorAttachments: [
            {
                attachment: color_texture.createView(),
                loadValue: {r: 0.3, g: 0.0, b: 0.2, a: 1.0}, // clear to dark purple
                storeOp: 'store'
            }
        ],
        depthStencilAttachment: {
            attachment: depth_texture.createView(),
            depthLoadValue: 0.0,
            depthStoreOp: 'store',
            depthReadOnly: false,
            stencilLoadValue: 0,
            stencilStoreOp: 'store',
            stencilReadOnly: true
        },
    });

    render_pass.setPipeline(render_pipeline);
    render_pass.setVertexBuffer(0, vertex_buffer);
    render_pass.setIndexBuffer(index_buffer);
    render_pass.setBindGroup(0, bind_group_common);
    render_pass.setBindGroup(1, bind_group_dynamic);
    render_pass.drawIndexed(index_data.length);
    render_pass.endPass();

    var command_buffer = command_enc.finish();
    queue.submit([command_buffer]);

    requestAnimationFrame(function() {
        render(device, swap_chain, queue, canvas, render_pipeline, vertex_buffer, index_buffer, bind_group_common, bind_group_dynamic, index_data);
    });
}

start();