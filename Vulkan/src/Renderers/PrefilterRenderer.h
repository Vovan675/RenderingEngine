#pragma once

#include "RendererBase.h"
#include "Mesh.h"
#include "Camera.h"
#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class PrefilterRenderer: public RendererBase
{
public:
	struct PushConstantFrag
	{
		float roughness = 0;
	} constants_frag;

	PrefilterRenderer();

	void addPass(FrameGraph &fg);

	RHITextureRef prefilter_texture;

private:
	RHIShaderRef compute_shader;
};

