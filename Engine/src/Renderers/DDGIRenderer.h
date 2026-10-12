#pragma once
#include "FrameGraph/FrameGraph.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "Rendering/Mesh.h"
#include "Rendering/ShaderStructs.h"

class DDGIRenderer
{
public:
	DDGIRenderer();

	void update();
	void addPasses(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addVisualizePass(FrameGraph &fg);

	static void readAtlases(RenderPassBuilder &builder);
	static void setAtlasConstants(const RenderPassResources &resources);

	const DDGIVolumeGPU &getVolume() const { return volume; }
private:
	eastl::vector<eastl::pair<const char *, const char *>> calculateDefines(eastl::vector<eastl::pair<const char *, const char *>> additional = {});

	void update_probes();

	void addPreparePass(FrameGraph &fg);
	void addTraceRaysPass(FrameGraph &fg, Ref<RayTracingScene> rt_scene);
	void addUpdatePass(FrameGraph &fg);
	void addRelocationPass(FrameGraph &fg);
	void addClassificationPass(FrameGraph &fg);

	DDGIVolumeGPU volume;
	bool last_use_relocation = false;
	bool last_use_classification = false;

	eastl::vector<uint32_t> probes_to_update;

	struct CascadeUpdateData
	{
		uint32_t last_local_index = 0;
	};
	eastl::vector<CascadeUpdateData> cascades_update {5};

	Engine::Mesh *sphere_mesh;
	RHIShaderRef visualize_vertex_shader;
	RHIShaderRef visualize_fragment_shader;
};
