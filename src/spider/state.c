#include "state.h"

#include <emscripten.h>
#include <emscripten/html5.h>

#include "debug.h"
#include "impl.h"

#include "scene_node.h"
#include "light.h"
#include "ubos.h"
#include "file.h"
#include "shader.h"
#include "imgui_impl_spider.h"

extern _SPState _sp_state;

void spInit(const SPInitDesc* desc) {
	SP_ASSERT(desc->update_func);
	_sp_state.device = emscripten_webgpu_get_device();
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: got device\n");
	wgpuDeviceSetUncapturedErrorCallback(_sp_state.device, &_spErrorCallback, NULL);

	_sp_state.queue = wgpuDeviceGetDefaultQueue(_sp_state.device);
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created queue\n");

	WGPUSurfaceDescriptorFromHTMLCanvasId canvas_desc = {
		.chain = {
			.sType = WGPUSType_SurfaceDescriptorFromHTMLCanvasId
		},
		.id = "canvas"
	};

	WGPUSurfaceDescriptor surf_desc = {
		.nextInChain = (WGPUChainedStruct const*)&canvas_desc, 
	};

	_sp_state.instance = NULL;  // null instance
	_sp_state.surface = wgpuInstanceCreateSurface(_sp_state.instance, &surf_desc);
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created surface\n");

	_sp_state.surface_size.width = _SP_GET_DEFAULT_IF_ZERO(desc->surface_size.width, _SP_SURFACE_WIDTH_DEFAULT);
	_sp_state.surface_size.height = _SP_GET_DEFAULT_IF_ZERO(desc->surface_size.height, _SP_SURFACE_HEIGHT_DEFAULT);

	_sp_state.update_func = desc->update_func;

	WGPUSwapChainDescriptor sc_desc = {
		.usage = WGPUTextureUsage_OutputAttachment,
		.format = WGPUTextureFormat_BGRA8Unorm,
		.width = desc->surface_size.width,
		.height = desc->surface_size.height,
		.presentMode = WGPUPresentMode_Fifo
	};
	_sp_state.swap_chain = wgpuDeviceCreateSwapChain(_sp_state.device, _sp_state.surface, &sc_desc);
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created swapChain\n");

	_sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
	
	_spSetupPools(&(_sp_state.pools), &(desc->pools));

	
	// Camera uniform buffer creation
	{
		WGPUBufferDescriptor buffer_desc = {
			.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
			.size = sizeof(_SPUboCamera),
		};

	   _sp_state.buffers.uniform.camera = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
	}

	_sp_state.dynamic_alignment = 256;

	// Model uniform buffer creation
	const uint32_t instance_count = (_sp_state.pools.scene_node.info.size - 1);
	{
		WGPUBufferDescriptor buffer_desc = {
			.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
			.size = _sp_state.dynamic_alignment * instance_count,
		};

		_sp_state.buffers.uniform.model = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
	}

	// Light uniform buffer creation
	const uint32_t light_count = (_sp_state.pools.light.info.size - 1);
	{
		WGPUBufferDescriptor buffer_desc = {
			.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
			.size = _sp_state.dynamic_alignment * light_count,
		};

		_sp_state.buffers.uniform.light = wgpuDeviceCreateBuffer(_sp_state.device, &buffer_desc);
	}

	_sp_state.active_cam = desc->camera;

	_spCreateForwardRenderPipeline();
	_spCreateShadowMapRenderPipeline();

	_sp_state.rm_counts_per_mat = SP_MALLOC(sizeof(uint32_t) * desc->pools.capacities.materials);
	_sp_state.sorted_rm = SP_MALLOC((sizeof(SPRenderMeshID) * desc->pools.capacities.materials) * desc->pools.capacities.render_meshes);
	_sp_state.dirty_nodes.data = SP_MALLOC(sizeof(SPSceneNode*) * desc->pools.capacities.scene_nodes);
	_sp_state.dirty_nodes.count = 0;

	WGPUTextureViewDescriptor tex_view_desc_linear_32 = {
		.format = WGPUTextureFormat_RGBA8Unorm,
		.dimension = WGPUTextureViewDimension_2D,
		.baseMipLevel = 0,
		.mipLevelCount = 0,
		.baseArrayLayer = 0,
		.arrayLayerCount = 0,
		.aspect = WGPUTextureAspect_All,
	};

	_SPTextureViewFromImageDescriptor default_image_descs[] = {
		{
			&(_sp_state.default_textures.normal),
			"assets/textures/default_normal_32.png",
			4,
			&tex_view_desc_linear_32
		},
		{
			&(_sp_state.default_textures.ao_roughness_metallic),
			"assets/textures/default_ao_roughness_metallic_32.png",
			4,
			&tex_view_desc_linear_32
		}
	};

	_spCreateAndLoadTextures(default_image_descs, SP_ARRAY_LEN(default_image_descs));
	DEBUG_PRINT(DEBUG_PRINT_TYPE_CREATE_MATERIAL, "init: created default texture views\n");

	_spImGuiInit(&(SPInitImGuiDesc){
		.display_size = {
			.width = _sp_state.surface_size.width,
			.height = _sp_state.surface_size.height,
		},
		.color_format = WGPUTextureFormat_BGRA8Unorm
	});

	_sp_state.show_stats = desc->show_stats;
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, _spEmscriptenKeyCallback);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, _spEmscriptenKeyCallback);
	emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, _spEmscriptenKeyCallback);

	emscripten_set_mousedown_callback("#canvas", NULL, true, _spEmscriptenMouseCallback);
	emscripten_set_mouseup_callback("#canvas", NULL, true, _spEmscriptenMouseCallback);
	emscripten_set_mousemove_callback("#canvas", NULL, true, _spEmscriptenMouseCallback);
}

void spShutdown(void) {
	_spImGuiShutdown(&_sp_state.imgui);

	for(uint32_t i = 0; i < _sp_state.pools.mesh.info.size; i++) {
		SPMesh* mesh = &(_sp_state.pools.mesh.data[i]);
		if(!mesh) {
			continue;
		}
		_SP_RELEASE_RESOURCE(Buffer, mesh->vertex_buffer)
		_SP_RELEASE_RESOURCE(Buffer, mesh->index_buffer)
	}
	_spDiscardPool(&_sp_state.pools.mesh.info);
	free(_sp_state.pools.mesh.data);
	_sp_state.pools.mesh.data = NULL;

	for(uint32_t i = 0; i < _sp_state.pools.material.info.size; i++) {
		SPMaterial* material = &(_sp_state.pools.material.data[i]);
		if(!material) {
			continue;
		}
		_SP_RELEASE_RESOURCE(BindGroup, material->bind_groups.vert)
		_SP_RELEASE_RESOURCE(BindGroup, material->bind_groups.frag)
		_SP_RELEASE_RESOURCE(Sampler, material->sampler)
		_SP_RELEASE_RESOURCE(Texture, material->albedo.texture)
		_SP_RELEASE_RESOURCE(TextureView, material->albedo.view)
		_SP_RELEASE_RESOURCE(Texture, material->ao_roughness_metallic.texture)
		_SP_RELEASE_RESOURCE(TextureView, material->ao_roughness_metallic.view)
	}
	_spDiscardPool(&_sp_state.pools.material.info);
	free(_sp_state.pools.material.data);
	_sp_state.pools.material.data = NULL;

	// RenderMeshes don't have resources that need to be released/freed
	_spDiscardPool(&_sp_state.pools.render_mesh.info);
	free(_sp_state.pools.render_mesh.data);
	_sp_state.pools.render_mesh.data = NULL;

	// Lights don't have resources that need to be released/freed
	_spDiscardPool(&_sp_state.pools.light.info);
	free(_sp_state.pools.light.data);
	_sp_state.pools.light.data = NULL;

	for(uint32_t i = 0; i < _sp_state.pools.scene_node.info.size; i++) {
		SPSceneNode* node = &(_sp_state.pools.scene_node.data[i]);
		if(!node) {
			continue;
		}
		if(node->tree.children.capacity > 1) {
			free(node->tree.children.list);
		}
	}
	_spDiscardPool(&_sp_state.pools.scene_node.info);
	free(_sp_state.pools.scene_node.data);
	_sp_state.pools.scene_node.data = NULL;

	wgpuBufferRelease(_sp_state.buffers.uniform.camera);
	wgpuBufferRelease(_sp_state.buffers.uniform.model);
	wgpuBufferRelease(_sp_state.buffers.uniform.light);

	free(_sp_state.sorted_rm);
	_sp_state.sorted_rm = NULL;
	free(_sp_state.rm_counts_per_mat);
	_sp_state.rm_counts_per_mat = NULL;

	free(_sp_state.dirty_nodes.data);
	_sp_state.dirty_nodes.data = NULL;

	_SP_RELEASE_RESOURCE(Device, _sp_state.device)
	_SP_RELEASE_RESOURCE(Queue, _sp_state.queue)
	_SP_RELEASE_RESOURCE(Instance, _sp_state.instance)
	_SP_RELEASE_RESOURCE(Surface, _sp_state.surface)
	_SP_RELEASE_RESOURCE(SwapChain, _sp_state.swap_chain)
	_SP_RELEASE_RESOURCE(CommandEncoder, _sp_state.cmd_enc)

	_SP_RELEASE_RESOURCE(RenderPipeline, _sp_state.pipelines.render.forward.pipeline)
	_SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.forward.vert.module)
	_SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.forward.vert.bind_group_layout)
	_SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.forward.frag.module)
	_SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.forward.frag.bind_group_layout)

	_SP_RELEASE_RESOURCE(RenderPipeline, _sp_state.pipelines.render.shadow.pipeline)
	_SP_RELEASE_RESOURCE(BindGroup, _sp_state.shadow_bind_group)
	_SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.shadow.vert.module)
	_SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.shadow.vert.bind_group_layout)
	_SP_RELEASE_RESOURCE(ShaderModule, _sp_state.pipelines.render.shadow.frag.module)
	_SP_RELEASE_RESOURCE(BindGroupLayout, _sp_state.pipelines.render.shadow.frag.bind_group_layout)

}

void spStart(void) {
	emscripten_set_main_loop(_spUpdate, _sp_state.fps_limit, false);
	_sp_state.start_clock = clock();
}

SPCamera* spGetActiveCamera() {
	return &(_sp_state.active_cam);
}

SPMaterial* spGetMaterial(SPMaterialID mat_id) {
	if(_spIsIDValid(mat_id.id, &_sp_state.pools.material.info)) {
		return  &(_sp_state.pools.material.data[mat_id.id]);
	}
	return NULL;
}

SPMesh* spGetMesh(SPMeshID mesh_id) {
	if(_spIsIDValid(mesh_id.id, &_sp_state.pools.mesh.info)) {
		return  &(_sp_state.pools.mesh.data[mesh_id.id]);
	}
	return NULL;
}

SPSceneNode* spGetSceneNode(SPSceneNodeID scene_node_id) {
	if(_spIsIDValid(scene_node_id.id, &_sp_state.pools.scene_node.info)) {
		return  &(_sp_state.pools.scene_node.data[scene_node_id.id]);
	}
	return NULL;
}

SPRenderMesh* spGetRenderMesh(SPRenderMeshID rm_id) {
	if(_spIsIDValid(rm_id.id, &_sp_state.pools.render_mesh.info)) {
		return  &(_sp_state.pools.render_mesh.data[rm_id.id]);
	}
	return NULL;
}

SPLight* spGetLight(SPLightID light_id) {
	if(_spIsIDValid(light_id.id, &_sp_state.pools.light.info)) {
		return  &(_sp_state.pools.light.data[light_id.id]);
	}
	return NULL;
}

SPKeyState spGetKeyState(SPKey key) {
	return _spInputGetKeyState(&_sp_state.input, key);
}
bool spGetKeyPressed(SPKey key) {
	return _spInputGetKeyState(&_sp_state.input, key) & SPKeyState_Pressed;
}

bool spGetKeyDown(SPKey key) {
	return _spInputGetKeyState(&_sp_state.input, key) & SPKeyState_Down;
}

bool spGetKeyUp(SPKey key) {
	return _spInputGetKeyState(&_sp_state.input, key) & SPKeyState_Up;
}

SPMouseButtonState spGetMouseButtonState(SPMouseButton button) {
	return _spInputGetMouseButtonState(&_sp_state.input, button);
}

bool spGetMouseButtonPressed(SPMouseButton button) {
	return _spInputGetMouseButtonState(&_sp_state.input, button) & SPMouseButtonState_Pressed;
}

bool spGetMouseButtonDown(SPMouseButton button) {
	return _spInputGetMouseButtonState(&_sp_state.input, button) & SPMouseButtonState_Down;
}

bool spGetMouseButtonUp(SPMouseButton button) {
	return _spInputGetMouseButtonState(&_sp_state.input, button) & SPMouseButtonState_Up;
}

uint32_t spGetMousePositionX() {
	return _sp_state.input.mouse_position.x;
}
uint32_t spGetMousePositionY() {
	return _sp_state.input.mouse_position.y;
}

void _spErrorCallback(WGPUErrorType type, char const * message, void * userdata) {
	printf("[%d] error(%d): %s", _sp_state.frame_index, (int)type, message);
}

int _spEmscriptenKeyCallback(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData) {
	_sp_state.input.modifiers = 
		(keyEvent->altKey ? SPInputModifiers_Alt : 0) |
		(keyEvent->ctrlKey ? SPInputModifiers_Control : 0) |
		(keyEvent->shiftKey ? SPInputModifiers_Shift : 0) |
		(keyEvent->metaKey ? SPInputModifiers_Meta : 0);
	if(eventType == EMSCRIPTEN_EVENT_KEYDOWN) {
		SPKey key = _spInputGetKeyForString(keyEvent->code);
		if(key == SPKey_None) {
			return 0;
		}
		if(!keyEvent->repeat) {
			_spInputSetKeyState(&_sp_state.input, key, SPKeyState_Down | SPKeyState_Pressed);
		}
	}
	else if(eventType == EMSCRIPTEN_EVENT_KEYUP) {
		SPKey key = _spInputGetKeyForString(keyEvent->code);
		if(key == SPKey_None) {
			return 0;
		}
		_spInputSetKeyState(&_sp_state.input, key, SPKeyState_Up);
	}
	else if(eventType == EMSCRIPTEN_EVENT_KEYPRESS) {
		memcpy(_sp_state.input.utf8_code, keyEvent->key, 32);
	}
	return 0;
}

int _spEmscriptenMouseCallback(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData) {
	if(eventType == EMSCRIPTEN_EVENT_MOUSEMOVE) {
		_sp_state.input.mouse_position.x = mouseEvent->targetX;
		_sp_state.input.mouse_position.y = mouseEvent->targetY;
		return 0;
	}
	if(eventType == EMSCRIPTEN_EVENT_MOUSEDOWN || eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
		SPMouseButton button = _spInputGetMouseButtonForId(mouseEvent->button);
		if(button == SPMouseButton_None) {
			return 0;
		}

		if(eventType == EMSCRIPTEN_EVENT_MOUSEDOWN) {
			_spInputSetMouseButtonState(&_sp_state.input, button, SPMouseButtonState_Down | SPMouseButtonState_Pressed);
			return 1; // Event consumed
		}
		else if(eventType == EMSCRIPTEN_EVENT_MOUSEUP) {
			_spInputSetMouseButtonState(&_sp_state.input, button, SPMouseButtonState_Up);
			return 1; // Event consumed
		}
	}
	return 0;
}

void _spCreateForwardRenderPipeline() {
	_sp_state.pipelines.render.forward = _spCreateRenderPipeline(&(_SPRenderPipelineDesc){
		.vertex_buffers = {
			{
				.array_stride = sizeof(SPVertex),
				.step_mode = WGPUInputStepMode_Vertex,
				.attributes = {
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float3,
						.offset = offsetof(SPVertex, pos),
						.shaderLocation = 0
					},
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float2,
						.offset = offsetof(SPVertex, tex_coords),
						.shaderLocation = 1
					},
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float3,
						.offset = offsetof(SPVertex, normal),
						.shaderLocation = 2
					},
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float3,
						.offset = offsetof(SPVertex, tangent),
						.shaderLocation = 3
					},
				}
			}
		},
		.uniform_block_count = 3,
		.vert = {
			.file = "src/shaders/compiled/forward.vert.spv",
			.entry = "main",
			.textures = {},
		},
		.frag = {
			.file = "src/shaders/compiled/forward_pbr.frag.spv",
			.entry = "main",
			.textures = {
				// albedo
				{
					.view_dimension = WGPUTextureViewDimension_2D,
					.texture_component_type = WGPUTextureComponentType_Float
				},
				// normal
				{
					.view_dimension = WGPUTextureViewDimension_2D,
					.texture_component_type = WGPUTextureComponentType_Float
				},
				// ao_roughness_metallic
				{
					.view_dimension = WGPUTextureViewDimension_2D,
					.texture_component_type = WGPUTextureComponentType_Float
				},
				// shadow
				{
					.view_dimension = WGPUTextureViewDimension_2D,
					.texture_component_type = WGPUTextureComponentType_Float
				}
			},
		},
		.primitive_topology = WGPUPrimitiveTopology_TriangleList,
		.index_format = WGPUIndexFormat_Uint16,
		.depth_stencil = {
			.depthWriteEnabled = true,
			.format = WGPUTextureFormat_Depth32Float,
			.depthCompare = WGPUCompareFunction_Greater,
			.stencilFront = {
				.compare = WGPUCompareFunction_Always,
				.failOp = WGPUStencilOperation_Keep,
				.depthFailOp = WGPUStencilOperation_Keep,
				.passOp = WGPUStencilOperation_Keep,
			},
			.stencilBack = {
				.compare = WGPUCompareFunction_Always,
				.failOp = WGPUStencilOperation_Keep,
				.depthFailOp = WGPUStencilOperation_Keep,
				.passOp = WGPUStencilOperation_Keep,
			},
			.stencilReadMask = 0xFFFFFFFF,
			.stencilWriteMask = 0xFFFFFFFF,
		},
		.rasterizer = {
			.frontFace = WGPUFrontFace_CW,
			.cullMode = WGPUCullMode_Front,
			.depthBias = 0,
			.depthBiasSlopeScale = 0.0f,
			.depthBiasClamp = 0.0f,
		},
		.color = {
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
			.writeMask = WGPUColorWriteMask_Red | WGPUColorWriteMask_Green | WGPUColorWriteMask_Blue
		}
	});;

	WGPUTextureDescriptor texture_desc = {
		.usage = WGPUTextureUsage_OutputAttachment,
		.dimension = WGPUTextureDimension_2D,
		.size = {
			.width = _sp_state.surface_size.width,
			.height = _sp_state.surface_size.height,
			.depth = 1,
		},
		.arrayLayerCount = 1, // TODO: deprecated, but needed for dawn
		.format = WGPUTextureFormat_Depth32Float,
		.mipLevelCount = 1,
		.sampleCount = 1,
	};
	WGPUTexture depth_texture = wgpuDeviceCreateTexture(_sp_state.device, &texture_desc);

	WGPUTextureViewDescriptor view_desc = {
		.format = WGPUTextureFormat_Depth32Float,
		.dimension = WGPUTextureViewDimension_2D,
		.aspect = WGPUTextureAspect_All,
	};
	_sp_state.depth_view = wgpuTextureCreateView(depth_texture, &view_desc);
}

void _spCreateShadowMapRenderPipeline() {
	_SPRenderPipeline* pipeline = &(_sp_state.pipelines.render.shadow);

	pipeline->vert.module = _spCreateShaderModuleFromSpirVFile("src/shaders/compiled/shadow.vert.spv");
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow: created vert shader\n");

	pipeline->frag.module = _spCreateShaderModuleFromSpirVFile("src/shaders/compiled/shadow.frag.spv");
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: shadow: created frag shader\n");

	WGPUBindGroupLayoutEntry vert_bgles[] = {
		{
			.binding = 0,
			.visibility = WGPUShaderStage_Vertex,
			.type = WGPUBindingType_UniformBuffer,
			.hasDynamicOffset = true,
			.multisampled = false,
			.viewDimension = WGPUTextureViewDimension_Undefined,
			.textureComponentType = WGPUTextureComponentType_Float,
		},
		{
			.binding = 1,
			.visibility = WGPUShaderStage_Vertex,
			.type = WGPUBindingType_UniformBuffer,
			.hasDynamicOffset = false,
			.multisampled = false,
			.viewDimension = WGPUTextureViewDimension_Undefined,
			.textureComponentType = WGPUTextureComponentType_Float,
		},
	};

	WGPUBindGroupLayoutDescriptor vert_bgl_desc = {
		.entryCount = SP_ARRAY_LEN(vert_bgles),
		.entries = vert_bgles
	};
	pipeline->vert.bind_group_layout = wgpuDeviceCreateBindGroupLayout(_sp_state.device, &vert_bgl_desc);
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: forward render: created vert bind group layout\n");

	WGPUBindGroupLayout bgls[] = {
		pipeline->vert.bind_group_layout
	};

	WGPUPipelineLayoutDescriptor pipeline_layout_desc = {
		.bindGroupLayoutCount = SP_ARRAY_LEN(bgls),
		.bindGroupLayouts = bgls
	};

	WGPUBindGroupEntry vert_entries[] = {
		{
			.binding = 0,
			.buffer = _sp_state.buffers.uniform.model,
			.offset = 0,
			.size = sizeof(_SPUboModel),
			.sampler = NULL,
			.textureView = NULL,
		},
		{
			.binding = 1,
			.buffer = _sp_state.buffers.uniform.light,
			.offset = 0,
			.size = sizeof(_SPUboLight),
			.sampler = NULL,
			.textureView = NULL,
		},
	};

	WGPUBindGroupDescriptor vert_bg_desc = {
		.layout = pipeline->vert.bind_group_layout,
		.entryCount = SP_ARRAY_LEN(vert_entries),
		.entries = vert_entries
	};
	_sp_state.shadow_bind_group = wgpuDeviceCreateBindGroup(_sp_state.device, &vert_bg_desc);

	WGPUVertexAttributeDescriptor vertex_attribute_descs[] = {
		{
			.format = WGPUVertexFormat_Float3,
			.offset = offsetof(SPVertex, pos),
			.shaderLocation = 0
		},
		{
			.format = WGPUVertexFormat_Float2,
			.offset = offsetof(SPVertex, tex_coords),
			.shaderLocation = 1
		},
		{
			.format = WGPUVertexFormat_Float3,
			.offset = offsetof(SPVertex, normal),
			.shaderLocation = 2
		},
		{
			.format = WGPUVertexFormat_Float3,
			.offset = offsetof(SPVertex, tangent),
			.shaderLocation = 3
		}
	};

	WGPUVertexBufferLayoutDescriptor vert_buffer_layout_descs[] = {
		{
			.arrayStride = sizeof(SPVertex),
			.stepMode = WGPUInputStepMode_Vertex,
			.attributeCount = SP_ARRAY_LEN(vertex_attribute_descs),
			.attributes = vertex_attribute_descs
		}
	};

	WGPUVertexStateDescriptor vert_state_desc = {
		.indexFormat = WGPUIndexFormat_Uint16,
		.vertexBufferCount = SP_ARRAY_LEN(vert_buffer_layout_descs),
		.vertexBuffers = vert_buffer_layout_descs
	};

	WGPUProgrammableStageDescriptor frag_stage_desc = {
		.module = pipeline->frag.module,
		.entryPoint = "main"
	};

	WGPUDepthStencilStateDescriptor depth_stencil_state_desc = {
		.depthWriteEnabled = true,
		.format = WGPUTextureFormat_Depth32Float,
		.depthCompare = WGPUCompareFunction_Greater,
		.stencilFront = {
			.compare = WGPUCompareFunction_Always,
			.failOp = WGPUStencilOperation_Keep,
			.depthFailOp = WGPUStencilOperation_Keep,
			.passOp = WGPUStencilOperation_Keep,
		},
		.stencilBack = {
			.compare = WGPUCompareFunction_Always,
			.failOp = WGPUStencilOperation_Keep,
			.depthFailOp = WGPUStencilOperation_Keep,
			.passOp = WGPUStencilOperation_Keep,
		},
		.stencilReadMask = 0xFFFFFFFF,
		.stencilWriteMask = 0xFFFFFFFF,
	};

	WGPURasterizationStateDescriptor rast_state_desc = {
		.frontFace = WGPUFrontFace_CW,
		.cullMode = WGPUCullMode_Front,
		.depthBias = 1, // Depth bias is not yet implemented in Chromium/dawn
		.depthBiasSlopeScale = 1.75f,
		.depthBiasClamp = 0.0f,
	};

	WGPUColorStateDescriptor color_state_desc = {
		.format = WGPUTextureFormat_R32Float,
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

	WGPURenderPipelineDescriptor pipeline_desc = {
		.layout = wgpuDeviceCreatePipelineLayout(_sp_state.device, &pipeline_layout_desc),
		.vertexStage.module = pipeline->vert.module,
		.vertexStage.entryPoint = "main",
		.vertexState = &vert_state_desc,
		.fragmentStage = &frag_stage_desc, // enable "No Color Output" mode
		.rasterizationState = &rast_state_desc,
		.sampleCount = 1,
		.depthStencilState = &depth_stencil_state_desc,
		.colorStateCount = 1,
		.colorStates = &color_state_desc,
		.primitiveTopology = WGPUPrimitiveTopology_TriangleList,
		.sampleMask = 0xFFFFFFFF
	};
	pipeline->pipeline = wgpuDeviceCreateRenderPipeline(_sp_state.device, &pipeline_desc);
	DEBUG_PRINT(DEBUG_PRINT_TYPE_INIT, "init: created shadow dir pipeline\n");
}

void _spCreateMipmapsComputePipeline() {

}

// Pool implementation from https://github.com/floooh/sokol/ 
// ***
void _spSetupPools(_SPPools* pools, const SPPoolsDesc* pools_desc) {
	_spInitPool(&(_sp_state.pools.material.info), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.materials, _SP_MATERIAL_POOL_DEFAULT));
	size_t mat_pool_byte_size = sizeof(SPMaterial) * pools->material.info.size;
	pools->material.data = (SPMaterial*) SP_MALLOC(mat_pool_byte_size);
	SP_ASSERT(pools->material.data);
	memset(pools->material.data, 0, mat_pool_byte_size);

	_spInitPool(&(_sp_state.pools.mesh.info), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.meshes, _SP_MESH_POOL_DEFAULT));
	size_t mesh_pool_byte_size = sizeof(SPMesh) * pools->mesh.info.size;
	pools->mesh.data = (SPMesh*) SP_MALLOC(mesh_pool_byte_size);
	SP_ASSERT(pools->mesh.data);
	memset(pools->mesh.data, 0, mesh_pool_byte_size);

	_spInitPool(&(_sp_state.pools.render_mesh.info), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.render_meshes, _SP_RENDER_MESH_POOL_DEFAULT));
	size_t render_mesh_pool_byte_size = sizeof(SPRenderMesh) * pools->render_mesh.info.size;
	pools->render_mesh.data = (SPRenderMesh*) SP_MALLOC(render_mesh_pool_byte_size);
	SP_ASSERT(pools->render_mesh.data);
	memset(pools->render_mesh.data, 0, render_mesh_pool_byte_size);

	_spInitPool(&(_sp_state.pools.light.info), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.lights, _SP_LIGHT_POOL_DEFAULT));
	size_t light_pool_byte_size = sizeof(SPLight) * pools->light.info.size;
	pools->light.data = (SPLight*) SP_MALLOC(light_pool_byte_size);
	SP_ASSERT(pools->light.data);
	memset(pools->light.data, 0, light_pool_byte_size);

	_spInitPool(&(_sp_state.pools.scene_node.info), _SP_GET_DEFAULT_IF_ZERO(pools_desc->capacities.scene_nodes, _SP_SCENE_NODE_POOL_DEFAULT));
	size_t scene_node_pool_byte_size = sizeof(SPSceneNode) * pools->scene_node.info.size;
	pools->scene_node.data = (SPSceneNode*) SP_MALLOC(scene_node_pool_byte_size);
	SP_ASSERT(pools->scene_node.data);
	memset(pools->scene_node.data, 0, scene_node_pool_byte_size);
}

void _spInitPool(_SPPool* pool, size_t size) {
	SP_ASSERT(pool && size >= 1);
	pool->size = size + 1;
	pool->queue_top = 0;
	size_t gen_ctrs_size = sizeof(uint32_t) * pool->size;
	pool->gen_ctrs = (uint32_t*) SP_MALLOC(gen_ctrs_size);
	SP_ASSERT(pool->gen_ctrs);
	pool->free_queue = (uint32_t*)SP_MALLOC(sizeof(uint32_t)*size);
	SP_ASSERT(pool->free_queue);
	/* never allocate the zero-th pool item since the invalid id is 0 */
	for (uint32_t i = pool->size-1; i >= 1; i--) {
		pool->free_queue[pool->queue_top++] = i;
	}
	pool->last_index_plus_1 = 1;
}

void _spDiscardPool(_SPPool* pool) {
	SP_ASSERT(pool);
	SP_ASSERT(pool->free_queue);
	SP_ASSERT(pool->free_queue);
	pool->free_queue = 0;
	SP_ASSERT(pool->gen_ctrs);
	SP_ASSERT(pool->gen_ctrs);
	pool->gen_ctrs = 0;
	pool->size = 0;
	pool->queue_top = 0;
}

uint32_t _spAllocPoolIndex(_SPPool* pool) {
	SP_ASSERT(pool);
	SP_ASSERT(pool->free_queue);
	if (pool->queue_top > 0) {
		uint32_t slot_index = pool->free_queue[--pool->queue_top];
		SP_ASSERT((slot_index > 0) && (slot_index < pool->size));
		if(slot_index + 1 > pool->last_index_plus_1) {
			pool->last_index_plus_1 = slot_index + 1;
		}
		return slot_index;
	}
	else {
		/* pool exhausted */
		return SP_INVALID_ID;
	}
}

void _spFreePoolIndex(_SPPool* pool, int slot_index) {
	SP_ASSERT((slot_index > SP_INVALID_ID) && (slot_index < pool->size));
	SP_ASSERT(pool);
	SP_ASSERT(pool->free_queue);
	SP_ASSERT(pool->queue_top < pool->size);
	#ifdef SPIDER_DEBUG
	/* debug check against double-free */
	for (uint32_t i = 0; i < pool->queue_top; i++) {
		SP_ASSERT(pool->free_queue[i] != slot_index);
	}
	#endif
	pool->free_queue[pool->queue_top++] = slot_index;
	SP_ASSERT(pool->queue_top <= (pool->size-1));
	if(slot_index + 1 == pool->last_index_plus_1) {
		pool->last_index_plus_1--;
	}
}

void _spUpdateDirtyNodes(void) {
	for(uint32_t i = 0; i < _sp_state.dirty_nodes.count; i++) {
		_spSceneNodeUpdateWorldTransform(_sp_state.dirty_nodes.data[i]);
	}
	_sp_state.dirty_nodes.count = 0;
}

void _spUpdateView(void) {
	vec3 up = { 0.0f, 1.0f, 0.0f };
	if(_sp_state.active_cam.mode == SPCameraMode_Direction) {
		glm_look(
			_sp_state.active_cam.pos,
			_sp_state.active_cam.dir,
			up,
			_sp_state.active_cam._view
		);
	} 
	else {
		glm_lookat(
			_sp_state.active_cam.pos,
			_sp_state.active_cam.look_at,
			up,
			_sp_state.active_cam._view
		);
	}
}

void _spUpdateProjection(void) {
   _spPerspectiveMatrixReversedZInfiniteFar(
		_sp_state.active_cam.fovy,
		_sp_state.active_cam.aspect,
		_sp_state.active_cam.near,
		_sp_state.active_cam._proj
	);
}

void _spUpdate(void) {
    clock_t cur_clock = clock();
    float delta_time_s = ((float)(cur_clock - _sp_state.start_clock) / CLOCKS_PER_SEC);
    _sp_state.start_clock = cur_clock;

	// ImGuiNewFrame should be first to allow (debug) UIs in each update function
	_spImGuiNewFrame(_sp_state.surface_size.width, _sp_state.surface_size.height, delta_time_s);

	_spInputResetKeyStates(&_sp_state.input);

	static bool input_open = true;
	igBegin("Input Tester", &input_open, ImGuiWindowFlags_None);
	if(igCollapsingHeaderTreeNodeFlags("Keyboard", ImGuiTreeNodeFlags_None)) {
		igInputText("Text input", _sp_state.test_buffer, 100, ImGuiInputTextFlags_None, NULL, NULL);
		igPushItemFlag(ImGuiItemFlags_Disabled, true);
		for(uint32_t key = 1; key < _SP_INPUT_KEY_COUNT; key++) {
			SPKeyState key_state = _spInputGetKeyState(&_sp_state.input, key);
			bool is_pressed = key_state & SPKeyState_Pressed;
			igCheckbox(_spInputGetStringForKey(key), &is_pressed);
		}
		igPopItemFlag();
	}
	if(igCollapsingHeaderTreeNodeFlags("Mouse", ImGuiTreeNodeFlags_None)) {
		igPushItemFlag(ImGuiItemFlags_Disabled, true);
		int mouse_pos[2] = {
			(int)_sp_state.input.mouse_position.x,
			(int)_sp_state.input.mouse_position.y
		};
		igSliderInt2("Position", mouse_pos, 0, _sp_state.surface_size.width, "%u");
		for(uint32_t button = 1; button < _SP_INPUT_MOUSE_BUTTON_COUNT; button++) {
			SPMouseButtonState button_state = _spInputGetMouseButtonState(&_sp_state.input, button);
			bool is_pressed = button_state & SPMouseButtonState_Pressed;
			igCheckbox(_spInputGetStringForMouseButton(button), &is_pressed);
		}
		igPopItemFlag();
	}
	igEnd();

	igShowMetricsWindow(&_sp_state.show_stats);
	bool should_continue = _sp_state.update_func(delta_time_s);

	_spUpdateDirtyNodes();
	_spUpdateView();
	_spUpdateProjection();
	_spUpdateUboCamera();
	_spUpdateUboModel();
	_spUpdateUboLight();

	_spRender();
}


void _spRender(void) {
	DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: start\n");

	WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(_sp_state.swap_chain);
	_spSortRenderMeshes();

	// shadow maps
	// ***
	{
		SPLight* light = spGetLight((SPLightID){1}); // TODO: currently just 1 light supported
		if(light) {
			
			// TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
			// remove color attachment when vertex-only render pipelines are available
			WGPURenderPassColorAttachmentDescriptor color_attachment = {
				.attachment = light->color_view,
				.loadOp = WGPULoadOp_Clear,
				.storeOp = WGPUStoreOp_Store,
				.clearColor = (WGPUColor){0.0f, 0.0f, 0.0, 1.0f}
			};

			WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
				.attachment = light->depth_view,
				.depthLoadOp = WGPULoadOp_Clear,
				.depthStoreOp = WGPUStoreOp_Store,
				.clearDepth = 0.0f,
				.stencilLoadOp = WGPULoadOp_Clear,
				.stencilStoreOp = WGPUStoreOp_Store,
				.clearStencil = 0,
			};

			// TODO: [vertex-only, dawn] https://bugs.chromium.org/p/dawn/issues/detail?id=1367
			// remove color attachment when vertex-only render pipelines are available
			WGPURenderPassDescriptor render_pass = {
				.colorAttachmentCount = 1,
				.colorAttachments = &color_attachment,
				.depthStencilAttachment = &depth_attachment,
			};
			WGPURenderPassEncoder shadow_pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
			wgpuRenderPassEncoderSetPipeline(shadow_pass_enc, _sp_state.pipelines.render.shadow.pipeline);
			SPMesh* last_mesh = NULL;
			for(SPRenderMeshID rm_id = {1}; rm_id.id < _sp_state.pools.render_mesh.info.size; rm_id.id++) {
				SPRenderMesh* rm = spGetRenderMesh(rm_id);
				if(rm) {
					SPMesh* mesh = rm->_mesh;
					if(mesh) {
						if(last_mesh != mesh) {
							wgpuRenderPassEncoderSetVertexBuffer(shadow_pass_enc, 0, mesh->vertex_buffer, 0, 0);
							wgpuRenderPassEncoderSetIndexBuffer(shadow_pass_enc, mesh->index_buffer, 0, 0);
							last_mesh = mesh;
						}
						uint32_t offsets_vert[] = { (rm_id.id - 1) * _sp_state.dynamic_alignment};
						wgpuRenderPassEncoderSetBindGroup(shadow_pass_enc, 0, _sp_state.shadow_bind_group, SP_ARRAY_LEN(offsets_vert), offsets_vert);
						wgpuRenderPassEncoderDrawIndexed(shadow_pass_enc, mesh->indices_count, 1, 0, 0, 0);
					}
				}
			}
			wgpuRenderPassEncoderEndPass(shadow_pass_enc);
			wgpuRenderPassEncoderRelease(shadow_pass_enc);
			DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
		}
	}
	// ***

	// main pass
	// ***
	{
		WGPURenderPassColorAttachmentDescriptor color_attachment = {
			.attachment = view,
			.loadOp = WGPULoadOp_Clear,
			.storeOp = WGPUStoreOp_Store,
			.clearColor = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f}
		};

		WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
			.attachment = _sp_state.depth_view,
			.depthLoadOp = WGPULoadOp_Clear,
			.depthStoreOp = WGPUStoreOp_Store,
			.clearDepth = 0.0f,
			.stencilLoadOp = WGPULoadOp_Clear,
			.stencilStoreOp = WGPUStoreOp_Store,
			.clearStencil = 0,
		};

		WGPURenderPassDescriptor render_pass = {
			.colorAttachmentCount = 1,
			.colorAttachments = &color_attachment,
			.depthStencilAttachment = &depth_attachment,
		};
		_sp_state.pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
		wgpuRenderPassEncoderSetPipeline(_sp_state.pass_enc, _sp_state.pipelines.render.forward.pipeline);
							

		for(SPMaterialID mat_id = {1}; mat_id.id < _sp_state.pools.material.info.size; mat_id.id++) {
			if(_sp_state.rm_counts_per_mat[mat_id.id - 1] == 0) {
				continue;
			}
			SPMaterial* material = spGetMaterial(mat_id);
			if(material) {
				wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 1, material->bind_groups.vert, 0, NULL);
				wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 2, material->bind_groups.frag, 0, NULL);
				SPMesh* last_mesh = NULL;
				uint32_t rm_count = _sp_state.rm_counts_per_mat[mat_id.id - 1];
				for(uint32_t i = 0; i < rm_count; i++) {
					SPRenderMeshID rm_id = _sp_state.sorted_rm[(mat_id.id - 1) * (_sp_state.pools.render_mesh.info.size - 1) + i];
					SPRenderMesh* rm = spGetRenderMesh(rm_id);
					if(rm) {
						SPMesh* mesh = rm->_mesh;
						if(mesh) {
							if(last_mesh != mesh) {
								wgpuRenderPassEncoderSetVertexBuffer(_sp_state.pass_enc, 0, mesh->vertex_buffer, 0, 0);
								wgpuRenderPassEncoderSetIndexBuffer(_sp_state.pass_enc, mesh->index_buffer, 0, 0);
								last_mesh = mesh;
							}
							uint32_t offsets_uniform[] = { (rm_id.id - 1) * _sp_state.dynamic_alignment, 0, 0};
							wgpuRenderPassEncoderSetBindGroup(_sp_state.pass_enc, 0, material->bind_groups.uniform, SP_ARRAY_LEN(offsets_uniform), offsets_uniform);
							wgpuRenderPassEncoderDrawIndexed(_sp_state.pass_enc, mesh->indices_count, 1, 0, 0, 0);
						}
					}
				}
			}
		}

		wgpuRenderPassEncoderEndPass(_sp_state.pass_enc);
		wgpuRenderPassEncoderRelease(_sp_state.pass_enc);
		DEBUG_PRINT(DEBUG_PRINT_RENDER, "render: finished render pass\n");
		
	}
	// ***

	_spImGuiRender(view);
	_spSubmit();
	
	wgpuTextureViewRelease(view);
	_sp_state.frame_index++;
}

void _spSubmit(void) {
	WGPUCommandBuffer commands = wgpuCommandEncoderFinish(_sp_state.cmd_enc, NULL);
	wgpuCommandEncoderRelease(_sp_state.cmd_enc);
	_sp_state.cmd_enc = NULL;

	wgpuQueueSubmit(_sp_state.queue, 1, &commands);
	wgpuCommandBufferRelease(commands);

	_sp_state.cmd_enc = wgpuDeviceCreateCommandEncoder(_sp_state.device, NULL);
}


void _spSortRenderMeshes(void) {
	uint32_t materials_count = _sp_state.pools.material.info.size - 1;
	memset(_sp_state.rm_counts_per_mat, 0, sizeof(uint32_t) * materials_count);
	for(SPRenderMeshID rm_id = {1}; rm_id.id < _sp_state.pools.render_mesh.info.size; rm_id.id++) {
		SPRenderMesh* rm = spGetRenderMesh(rm_id);
		if(rm) {
			SPMeshID mesh_id = rm->mesh_id;
			SPMaterialID mat_id = rm->material_id;
			if(mesh_id.id != SP_INVALID_ID && mat_id.id != SP_INVALID_ID) {
				_sp_state.sorted_rm[(mat_id.id - 1) * (_sp_state.pools.render_mesh.info.size - 1) + _sp_state.rm_counts_per_mat[mat_id.id - 1]++] = rm_id;
			}
		}
	}
}

bool _spIsIDValid(uint32_t id, const _SPPool* pool) {
	if(id == SP_INVALID_ID || id > pool->size) {
		return false;
	}
	for(uint32_t i = 0; i < pool->queue_top; i++) {
		if(id == pool->free_queue[i]) {
			return false; // object at id is not allocated
		}
	} 
	return true;
}

// ***