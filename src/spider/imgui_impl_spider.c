#include "imgui_impl_spider.h"

#include <webgpu/webgpu.h>

#include "impl.h"
#include "debug.h"
#include "shader.h"
#include "buffer.h"
#include "state.h"

extern _SPState _sp_state;

void _spImGuiInit(const SPInitImGuiDesc* desc) {
	_sp_state.imgui_state.max_vertices = _SP_GET_DEFAULT_IF_ZERO(desc->max_vertices, _SP_IMGUI_MAX_VERTICES_DEFAULT);
	_sp_state.imgui_state.display_size.width = desc->display_size.width;
	_sp_state.imgui_state.display_size.height = desc->display_size.height;

	igCreateContext(NULL);
	igStyleColorsDark(igGetStyle());
	ImGuiIO* io = igGetIO();
	ImFontAtlas_AddFontDefault(io->Fonts, NULL);
	io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

	_sp_state.imgui_state.vertex_buffer = _spCreateGpuBuffer(&(_SPGpuBufferDesc){
		.label = "imgui-vertex-buffer",
		.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst,
		.size = _sp_state.imgui_state.max_vertices * sizeof(ImDrawVert),
	});
	_sp_state.imgui_state.index_buffer = _spCreateGpuBuffer(&(_SPGpuBufferDesc){
		.label = "imgui-index-buffer",
		.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst,
		.size = _sp_state.imgui_state.max_vertices * 3 * sizeof(ImDrawIdx),
	});

	// Load font texture into WGPUTexture
	{
		unsigned char* font_pixels;
		int font_width;
		int font_height;
		int bytes_per_pixel;
		ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &font_pixels, &font_width, &font_height, &bytes_per_pixel);
		const uint32_t pixels_size_bytes = font_width * font_height * bytes_per_pixel;
		
		WGPUExtent3D texture_size = {
			.width = font_width,
			.height = font_height,
			.depth = 1,
		};
		WGPUTextureDescriptor texture_desc = {
			.label = "imgui-font-texture",
			.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_Sampled,
			.dimension = WGPUTextureDimension_2D,
			.size = texture_size,
			.arrayLayerCount = 1,
			.format = WGPUTextureFormat_RGBA8Unorm,
			.mipLevelCount = 1,
			.sampleCount = 1,
		};

		_sp_state.imgui_state.font_texture = wgpuDeviceCreateTexture(_sp_state.device, &texture_desc);
		SP_ASSERT(_sp_state.imgui_state.font_texture);

		WGPUBufferDescriptor buffer_desc = {
			.usage = WGPUBufferUsage_CopySrc,
			.size = pixels_size_bytes,
		};

		WGPUCreateBufferMappedResult result = wgpuDeviceCreateBufferMapped(_sp_state.device, &buffer_desc);
		SP_ASSERT(result.buffer && result.data);
		memcpy(result.data, font_pixels, result.dataLength);
		wgpuBufferUnmap(result.buffer);

		WGPUBufferCopyView buffer_copy_view = {
			.buffer = result.buffer,
			.offset = 0,
			.bytesPerRow = font_width * bytes_per_pixel,
			.rowsPerImage = font_height
		};

		WGPUTextureCopyView texture_copy_view = {
            .texture = _sp_state.imgui_state.font_texture,
            .mipLevel = 0,
            .arrayLayer = 0,
            .origin = {0, 0, 0},
        };

		wgpuCommandEncoderCopyBufferToTexture(
			_sp_state.cmd_enc, 
			&buffer_copy_view,
			&texture_copy_view,
			&texture_size
		);
		
		io->Fonts->TexID = 0;

		WGPUTextureViewDescriptor texture_view_desc = {
			.label = "imgui-texture-view",
			.format = WGPUTextureFormat_RGBA8Unorm,
			.dimension = WGPUTextureViewDimension_2D,
			.baseMipLevel = 0,
			.mipLevelCount = 1,
			.baseArrayLayer = 0,
			.arrayLayerCount = 1,
			.aspect = WGPUTextureAspect_All,
		};

		_sp_state.imgui_state.font_texture_view = wgpuTextureCreateView(_sp_state.imgui_state.font_texture, &texture_view_desc);
		SP_ASSERT(_sp_state.imgui_state.font_texture_view);

		WGPUSamplerDescriptor sampler_desc = {
			.label = "imgui-font-sampler",
			.addressModeU = WGPUAddressMode_ClampToEdge,
			.addressModeV = WGPUAddressMode_ClampToEdge,
			.addressModeW = WGPUAddressMode_ClampToEdge,
			.magFilter = WGPUFilterMode_Linear,
			.minFilter = WGPUFilterMode_Linear,
			.mipmapFilter = WGPUFilterMode_Linear,
			.lodMinClamp = 0.0f,
			.lodMaxClamp = 1.0f,
			.compare = WGPUCompareFunction_Undefined,
		};

		_sp_state.imgui_state.font_sampler = wgpuDeviceCreateSampler(_sp_state.device, &sampler_desc);
		SP_ASSERT(_sp_state.imgui_state.font_sampler);
	}
	// Pipeline creation
	_sp_state.imgui_state.pipeline = _spCreateRenderPipeline(&(_SPRenderPipelineDesc){
		.vertex_buffers = {
			{
				.array_stride = sizeof(ImDrawVert),
				.step_mode = WGPUInputStepMode_Vertex,
				.attributes = {
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float2,
						.offset = offsetof(ImDrawVert, pos),
						.shaderLocation = 0
					},
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_Float2,
						.offset = offsetof(ImDrawVert, uv),
						.shaderLocation = 1
					},
					&(WGPUVertexAttributeDescriptor){
						.format = WGPUVertexFormat_UChar4Norm,
						.offset = offsetof(ImDrawVert, col),
						.shaderLocation = 2
					},
				}
			}
		},
		.uniform_block_count = 1,
		.vert = {
			.file = "src/shaders/compiled/imgui.vert.spv",
			.entry = "main",
			.textures = {},
		},
		.frag = {
			.file = "src/shaders/compiled/imgui.frag.spv",
			.entry = "main",
			.textures = {
				{
					.view_dimension = WGPUTextureViewDimension_2D,
					.texture_component_type = WGPUTextureComponentType_Float
				}
			},
		},
		.primitive_topology = WGPUPrimitiveTopology_TriangleList,
		.index_format = WGPUIndexFormat_Uint16,
		.depth_stencil = {
			.depthWriteEnabled = false,
			.format = WGPUTextureFormat_Depth32Float,
			.depthCompare = WGPUCompareFunction_Always,
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
			.stencilReadMask = 0x0,
			.stencilWriteMask = 0x0,
		},
		.rasterizer = {
			.frontFace = WGPUFrontFace_CW,
			.cullMode = WGPUCullMode_None,
			.depthBias = 0,
			.depthBiasSlopeScale = 0.0f,
			.depthBiasClamp = 0.0f,
		},
		.color = {
			.format = desc->color_format,
			.alphaBlend = {
				.operation = WGPUBlendOperation_Add,
				.srcFactor = WGPUBlendFactor_One,
				.dstFactor = WGPUBlendFactor_Zero,
			},
			.colorBlend = {
				.operation = WGPUBlendOperation_Add,
				.srcFactor = WGPUBlendFactor_SrcAlpha,
				.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
			},
			.writeMask = WGPUColorWriteMask_Red | WGPUColorWriteMask_Green | WGPUColorWriteMask_Blue
		}
	});

	// Create uniform bind group
	{
		ImVec2 display_size = {
			(float)desc->display_size.width, 
			(float)desc->display_size.height
		};
		_sp_state.imgui_state.uniform_buffer = _spCreateGpuBuffer(&(_SPGpuBufferDesc){
			.label = "imgui-uniform-buffer",
			.usage = WGPUBufferUsage_Uniform,
			.size = sizeof(ImVec2),
			.initial = {
				.data = &display_size,
				.size = sizeof(ImVec2),
			}
		});

		WGPUBindGroupEntry bg_entries[] = {
			{
				.binding = 0,
				.offset = 0,
				.size = sizeof(ImVec2),
				.buffer = _sp_state.imgui_state.uniform_buffer.buffer,
			}
		};

		WGPUBindGroupDescriptor bg_desc = {
			.layout = _sp_state.imgui_state.pipeline.uniform_bind_group_layout,
			.entryCount = SP_ARRAY_LEN(bg_entries),
			.entries = bg_entries,
		};

		_sp_state.imgui_state.bind_groups.uniform = wgpuDeviceCreateBindGroup(_sp_state.device, &bg_desc);
		SP_ASSERT(_sp_state.imgui_state.bind_groups.uniform);
	}

	// Create vertex bind group
	{
		WGPUBindGroupEntry bg_entries[] = {};

		WGPUBindGroupDescriptor bg_desc = {
			.layout = _sp_state.imgui_state.pipeline.vert.bind_group_layout,
			.entryCount = SP_ARRAY_LEN(bg_entries),
			.entries = bg_entries,
		};

		_sp_state.imgui_state.bind_groups.vert = wgpuDeviceCreateBindGroup(_sp_state.device, &bg_desc);
		SP_ASSERT(_sp_state.imgui_state.bind_groups.vert);
	}

	// Create fragment bind group
	{
		WGPUBindGroupEntry bg_entries[] = {
			{
				.binding = 0,
				.offset = 0,
				.size = 0,
				.textureView = _sp_state.imgui_state.font_texture_view,
			},
			{
				.binding = 1,
				.offset = 0,
				.size = 0,
				.sampler = _sp_state.imgui_state.font_sampler,
			}
		};

		WGPUBindGroupDescriptor bg_desc = {
			.layout = _sp_state.imgui_state.pipeline.frag.bind_group_layout,
			.entryCount = SP_ARRAY_LEN(bg_entries),
			.entries = bg_entries,
		};

		_sp_state.imgui_state.bind_groups.frag = wgpuDeviceCreateBindGroup(_sp_state.device, &bg_desc);
		SP_ASSERT(_sp_state.imgui_state.bind_groups.frag);
	}

}

void _spImGuiShutdown(_SPImGuiState* imgui) {
    _SP_RELEASE_RESOURCE(ShaderModule, 		imgui->pipeline.vert.module);
	_SP_RELEASE_RESOURCE(BindGroupLayout, 	imgui->pipeline.vert.bind_group_layout);
	_SP_RELEASE_RESOURCE(ShaderModule, 		imgui->pipeline.frag.module);
	_SP_RELEASE_RESOURCE(BindGroupLayout, 	imgui->pipeline.frag.bind_group_layout);
    _SP_RELEASE_RESOURCE(Texture, 			imgui->font_texture);
	_SP_RELEASE_RESOURCE(TextureView,		imgui->font_texture_view);
	_SP_RELEASE_RESOURCE(Sampler, 			imgui->font_sampler);
    _SP_RELEASE_RESOURCE(Buffer, 			imgui->vertex_buffer.buffer);
	_SP_RELEASE_RESOURCE(Buffer, 			imgui->index_buffer.buffer);

	_SP_RELEASE_RESOURCE(Buffer, 			imgui->uniform_buffer.buffer);
	_SP_RELEASE_RESOURCE(BindGroup,			imgui->bind_groups.vert);
	_SP_RELEASE_RESOURCE(BindGroup,			imgui->bind_groups.frag);
}

void _spImGuiNewFrame(uint32_t width, uint32_t height, float delta_time) {
	ImGuiIO* io = igGetIO();

	io->DisplaySize.x = (float)width;
	io->DisplaySize.y = (float)height;
	io->DeltaTime = delta_time;

	// TODO: add input handling

	igNewFrame();
}

void _spImGuiRender(WGPUTextureView view) {
	igRender();
	ImDrawData* draw_data = igGetDrawData();
	ImGuiIO* io = igGetIO();

	if (!draw_data || draw_data->CmdListsCount == 0) {
		return;
	}

	// ----
	// Update vertex and index buffers
	ImDrawVert* vertex_data = SP_MALLOC(_sp_state.imgui_state.max_vertices * sizeof(ImDrawVert));
	uint32_t* vertex_offsets = SP_MALLOC(draw_data->CmdListsCount * sizeof(uint32_t));
	vertex_offsets[0] = 0;
	uint32_t vertex_data_offset = 0;

	ImDrawVert* index_data = SP_MALLOC(_sp_state.imgui_state.max_vertices * 3 * sizeof(ImDrawIdx));
	uint32_t* index_offsets = SP_MALLOC(draw_data->CmdListsCount * sizeof(uint32_t));
	index_offsets[0] = 0;
	uint32_t index_data_offset = 0;

	for(int cl_index = 0; cl_index < draw_data->CmdListsCount; cl_index++) {
		ImDrawList* cl = draw_data->CmdLists[cl_index];
		// Populate vertex data
		{
			const uint32_t size_bytes = cl->VtxBuffer.Size * sizeof(ImDrawVert);
			const ImDrawVert* data_ptr = cl->VtxBuffer.Data;
			if (data_ptr) {
				memcpy((char*)vertex_data + vertex_data_offset, data_ptr, size_bytes);
				//DEBUG_PRINT(DEBUG_PRINT_IMGUI, "(imgui) added %u vertices (%u bytes) at offset %u\n", cl->VtxBuffer.Size, size_bytes, vertex_data_offset);
				vertex_data_offset += size_bytes;
			}
			if(cl_index < draw_data->CmdListsCount - 1) {
				vertex_offsets[cl_index + 1] = vertex_offsets[cl_index] + cl->VtxBuffer.Size;
			}
		}

		// Populate index data
		{
			const uint32_t size_bytes = cl->IdxBuffer.Size * sizeof(ImDrawIdx);
			const ImDrawIdx* data_ptr = cl->IdxBuffer.Data;
			if (data_ptr) {
				memcpy((char*)index_data + index_data_offset, data_ptr, size_bytes);
				//DEBUG_PRINT(DEBUG_PRINT_IMGUI, "(imgui) added %u indices (%u bytes) at offset %u\n", cl->IdxBuffer.Size, size_bytes, index_data_offset);
				index_data_offset += size_bytes;
			}
			if(cl_index < draw_data->CmdListsCount - 1) {
				index_offsets[cl_index + 1] = index_offsets[cl_index] + cl->IdxBuffer.Size;
			}
		}
	}

	// Ensure copy size to be a multiple of 4 bytes
	// Size of vertex_data should automatically be a multiple of 4 bytes because sizeof(ImDrawVert) is a multiple of 4 bytes
	const uint32_t vertex_data_size = vertex_data_offset;
	SP_ASSERT(vertex_data_size % 4 == 0);

	// Size of index_data could be off by 2 bytes, because sizeof(ImDrawIdx) is 2 bytes
	if(index_data_offset % 4 != 0) {
		ImDrawIdx padding = 0;
		memcpy((void*)index_data + index_data_offset, &padding, sizeof(ImDrawIdx));
		index_data_offset += sizeof(ImDrawIdx);
	}
	const uint32_t index_data_size = index_data_offset;
	SP_ASSERT(index_data_size % 4 == 0);

	_spRecordCopyDataToBuffer(_sp_state.cmd_enc, _sp_state.imgui_state.vertex_buffer, 0, vertex_data, vertex_data_size);
	free(vertex_data);
	_spRecordCopyDataToBuffer(_sp_state.cmd_enc, _sp_state.imgui_state.index_buffer, 0, index_data, index_data_size);
	free(index_data);
	// ----

	WGPURenderPassColorAttachmentDescriptor color_attachment = {
		.attachment = view,
		.loadOp = WGPULoadOp_Load,
		.storeOp = WGPUStoreOp_Store,
		.clearColor = (WGPUColor){0.0f, 0.0f, 0.0f, 1.0f}
	};

	WGPURenderPassDepthStencilAttachmentDescriptor depth_attachment = {
		.attachment = _sp_state.depth_view,
		.depthLoadOp = WGPULoadOp_Load,
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
	
	WGPURenderPassEncoder pass_enc = wgpuCommandEncoderBeginRenderPass(_sp_state.cmd_enc, &render_pass);
	wgpuRenderPassEncoderSetPipeline(pass_enc, _sp_state.imgui_state.pipeline.pipeline);
	const uint32_t dynamic_offsets_uniform[] = {0}; 
	wgpuRenderPassEncoderSetBindGroup(pass_enc, 0, _sp_state.imgui_state.bind_groups.uniform, SP_ARRAY_LEN(dynamic_offsets_uniform), dynamic_offsets_uniform);
	wgpuRenderPassEncoderSetBindGroup(pass_enc, 1, _sp_state.imgui_state.bind_groups.vert, 0, NULL);
	wgpuRenderPassEncoderSetBindGroup(pass_enc, 2, _sp_state.imgui_state.bind_groups.frag, 0, NULL);
	wgpuRenderPassEncoderSetIndexBuffer(pass_enc, _sp_state.imgui_state.index_buffer.buffer, 0, 0);
	wgpuRenderPassEncoderSetVertexBuffer(pass_enc, 0, _sp_state.imgui_state.vertex_buffer.buffer, 0, 0);
	//wgpuRenderPassEncoderSetViewport(pass_enc, 0.0f, 0.0f, io->DisplaySize.x, io->DisplaySize.y, 1.0f, 0.0f);

	uint32_t cmd_list_count = draw_data->CmdListsCount;
	uint32_t draw_cmd_count = 0;
	for(int cl_index = 0; cl_index < cmd_list_count; cl_index++) {
		ImDrawList* cl = draw_data->CmdLists[cl_index];

		
		const uint32_t num_cmds = cl->CmdBuffer.Size;
		draw_cmd_count += num_cmds;
		for(uint32_t cmd_index = 0; cmd_index < num_cmds; cmd_index++) {
			ImDrawCmd* pcmd = &cl->CmdBuffer.Data[cmd_index];
			SP_ASSERT(pcmd->TextureId == io->Fonts->TexID);
			const uint32_t scissor_x = (uint32_t)pcmd->ClipRect.x;
			const uint32_t scissor_y = (uint32_t)pcmd->ClipRect.y;
			const uint32_t scissor_w = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
			const uint32_t scissor_h = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);

			wgpuRenderPassEncoderSetScissorRect(pass_enc, scissor_x, scissor_y, scissor_w, scissor_h);
			const uint32_t total_index_offset = index_offsets[cl_index] + pcmd->IdxOffset;
			const uint32_t total_vertex_offset = vertex_offsets[cl_index] + pcmd->VtxOffset;
			wgpuRenderPassEncoderDrawIndexed(pass_enc, pcmd->ElemCount, 1, total_index_offset, total_vertex_offset, 0);
			//DEBUG_PRINT(DEBUG_PRINT_IMGUI, "(imgui) draw %u elements with vertex/index offsets: %u / %u\n", pcmd->ElemCount, total_vertex_offset, total_index_offset);
		}
	}

	wgpuRenderPassEncoderEndPass(pass_enc);
	_SP_RELEASE_RESOURCE(RenderPassEncoder, pass_enc);

	free(vertex_offsets);
	free(index_offsets);
}