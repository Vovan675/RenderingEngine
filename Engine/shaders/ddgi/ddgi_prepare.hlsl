#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static DDGIVolume volume = ddgi_volume;

cbuffer Constants : register(b1)
{
	uint metadata_atlas_tex_id;
	uint reset_relocation;
	uint reset_classification;
};

[[vk::image_format("rgba16f")]]
static RWTexture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[metadata_atlas_tex_id];

[numthreads(32, 1, 1)]
void CS_ResetMetadata(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	uint probe_id = dispatch_thread_id.x;
	uint cascade_id = GetProbeCascade(volume, probe_id);
	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint3 texel_coord = uint3(GetProbeStartTexelCoords(volume, probe_coord), cascade_id);

	float4 metadata = metadata_atlas[texel_coord];
	if (reset_relocation)
		metadata.xyz = float3(0, 0, 0);
	if (reset_classification)
		metadata.w = STATE_ENABLED;
	metadata_atlas[texel_coord] = metadata;
}
