#pragma once
#include "FrameGraph/FrameGraph.h"
#include "Rendering/ShaderStructs.h"
#include <Rtxdi/DI/ReSTIRDI.h>
#include "NRDDenoiser.h"

struct RTXDIConstants;

class RTXDIRenderer
{
public:
	void addPasses(FrameGraph &fg, LightTableLayout lights_layout);

private:
	void create_context();
	void set_context_parameters();
	void add_denoise_confidence_passes(FrameGraph &fg, const RTXDIConstants &frame_constants);
	void add_denoise_resolve_pass(FrameGraph &fg);

	std::unique_ptr<rtxdi::ReSTIRDIContext> context;
	RHIBufferRef neighbor_offsets_buffer;

	NRDDenoiser denoiser;
};
