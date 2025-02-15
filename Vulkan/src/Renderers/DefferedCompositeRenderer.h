#pragma once
#include "RendererBase.h"
#include "RHI/Pipeline.h"
#include "Mesh.h"
#include "RHI/Texture.h"
#include "Camera.h"
#include "RHI/Descriptors.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"

class DefferedCompositeRenderer: public RendererBase
{
public:
	DefferedCompositeRenderer();
	virtual ~DefferedCompositeRenderer();

	void addPasses(FrameGraph &fg);

	void renderImgui();
private:
};

