#pragma once
#include "FrameGraph/FrameGraph.h"

namespace UtilityPasses
{
	enum BlitBlend
	{
		BLIT_BLEND_OPAQUE,
		BLIT_BLEND_ALPHA,
	};

	void addBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, BlitBlend blend = BLIT_BLEND_OPAQUE, const wchar_t *shader = L"shaders/utility/blit.hlsl");
	void addDepthBlitPass(FrameGraph &fg, GraphicsResourceName src, GraphicsResourceName dst, glm::ivec2 dst_size);
}
