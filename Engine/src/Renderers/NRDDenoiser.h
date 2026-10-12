#pragma once
#include "FrameGraph/FrameGraph.h"
#include <NRD.h>

struct NRDRelaxInputs
{
	GraphicsResourceName diffuse_radiance_hit_distance;
	GraphicsResourceName specular_radiance_hit_distance;
	GraphicsResourceName diffuse_output;
	GraphicsResourceName specular_output;
	
	// Optional
	GraphicsResourceName diffuse_confidence;
	GraphicsResourceName specular_confidence;
};

using NRDResourceNames = eastl::array<GraphicsResourceName, (size_t)nrd::ResourceType::MAX_NUM>;

class NRDDenoiser
{
public:
	~NRDDenoiser();

	void addRelaxPasses(FrameGraph &fg, const NRDRelaxInputs &inputs);
	void addReferencePasses(FrameGraph &fg, const NRDRelaxInputs &inputs);

private:
	bool create_instance();
	void destroy_instance();
	void add_guide_passes(FrameGraph &fg);
	void set_common_settings(bool reset_history, bool validation, bool history_confidence);
	bool begin_frame(nrd::Identifier identifier);
	void add_dispatch_passes(FrameGraph &fg, nrd::Identifier identifier, const NRDResourceNames &resource_names);

	nrd::Instance *instance = nullptr;
	glm::ivec2 instance_resolution = glm::ivec2(0);

	eastl::vector<RHIShaderRef> shaders;

	uint64_t last_frame = 0;
	nrd::Identifier last_identifier = 0;
};
