#pragma once

#include "Rendering/GlobalPipeline.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraph.h"

class LutRenderer
{
public:
	LutRenderer();
	void addPasses(FrameGraph &fg);

	RHITextureRef brdf_lut_texture;
};

