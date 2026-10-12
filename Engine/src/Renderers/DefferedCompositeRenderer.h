#pragma once
#include "Rendering/GlobalPipeline.h"
#include "Rendering/Mesh.h"
#include "Utils/Camera.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "DDGIRenderer.h"

class DefferedCompositeRenderer
{
public:
	void addPasses(FrameGraph &fg);
};

