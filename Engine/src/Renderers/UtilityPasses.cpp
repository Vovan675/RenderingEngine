#include "pch.h"
#include "UtilityPasses.h"
#include "Rendering/GlobalPipeline.h"

void UtilityPasses::addBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, BlitBlend blend, const wchar_t *shader)
{
	fg.addCallbackPass("Blit Pass",
	[&](RenderPassBuilder &builder)
	{
		if (!builder.isTextureCreated(src))
			return;
		builder.readTexture(src);
		builder.writeTexture(dst);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		if (!resources.has(src))
			return;

		cmd_list->setRenderTargets({resources.getTexture(dst)}, nullptr, 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER), gDynamicRHI->createShader(shader, FRAGMENT_SHADER), VertexInputsDescription{}, blend != BLIT_BLEND_OPAQUE, false, CULL_MODE_BACK);
		if (blend == BLIT_BLEND_ALPHA)
			gGlobalPipeline->setBlendMode(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA, BLEND_OP_ADD, BLEND_ZERO, BLEND_ONE, BLEND_OP_ADD);
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t src_tex_id = resources.getReadTexture(src);
		gDynamicRHI->setConstantBufferData(0, &src_tex_id, sizeof(src_tex_id));

		cmd_list->drawInstanced(6, 1, 0, 0);
		cmd_list->resetRenderTargets();
	});
}

void UtilityPasses::addDepthBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, glm::ivec2 dst_size)
{
	fg.addCallbackPass("Depth Blit Pass",
	[&](RenderPassBuilder &builder)
	{
		builder.createTexture(dst, dst_size.x, dst_size.y, builder.getTextureDescription(src).format);
		builder.writeTexture(dst);
		builder.readTexture(src);
	},
	[=](const RenderPassResources &resources, RHICommandList *cmd_list)
	{
		cmd_list->setRenderTargets({}, resources.getTexture(dst), 0, 0, false);

		gGlobalPipeline->setupGraphicsPipeline(cmd_list, gDynamicRHI->createShader(L"shaders/quad.hlsl", VERTEX_SHADER), gDynamicRHI->createShader(L"shaders/utility/depth_blit.hlsl", FRAGMENT_SHADER), VertexInputsDescription{}, false, true, CULL_MODE_BACK);
		gGlobalPipeline->setDepthFunc(COMPARE_FUNC_ALWAYS);
		gGlobalPipeline->flushAndBind(cmd_list);

		uint32_t src_tex_id = resources.getReadTexture(src);
		gDynamicRHI->setConstantBufferData(0, &src_tex_id, sizeof(src_tex_id));

		cmd_list->drawInstanced(6, 1, 0, 0);
		cmd_list->resetRenderTargets();
	});
}
