#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static DDGIVolume volume = ddgi_volume;

cbuffer Constants : register(b1)
{
	uint output_atlas_tex_id;
	uint ray_data_buffer_id;
	uint probes_to_update_buffer_id;
	uint probes_to_update_count;
};

static StructuredBuffer<uint> probes_to_update = ResourceDescriptorHeap[probes_to_update_buffer_id];

[[vk::image_format("rgba16f")]]
static RWTexture2DArray<float4> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];

[numthreads(32, 1, 1)]
void CS_Classification(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
	if (dispatch_thread_id.x >= probes_to_update_count)
		return;

	uint probe_id = probes_to_update[dispatch_thread_id.x];
	uint cascade_id = GetProbeCascade(volume, probe_id);

	StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[ray_data_buffer_id];
	uint backface_count = 0;
	for (uint ray_index = 0; ray_index < volume.rays_per_probe; ray_index++)
	{
		uint ray_data_index = GetRayDataIndex(dispatch_thread_id.x, ray_index, volume);
		float ray_distance = ray_data_buffer[ray_data_index].a;

		if (ray_distance < 0)
		{
			backface_count++;
		}
	}

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint3 texel_coord = uint3(GetProbeStartTexelCoords(volume, probe_coord), cascade_id);

	// Disable probes if they have more than % backfaces
	bool is_inside_geometry = float(backface_count) / volume.rays_per_probe > BACKFACE_THRESHOLD;
	output_atlas[texel_coord].a = is_inside_geometry ? STATE_DISABLED : STATE_ENABLED;
}
