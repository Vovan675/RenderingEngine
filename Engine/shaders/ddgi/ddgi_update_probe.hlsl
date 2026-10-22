#include "../common.h"
#include "../bindless.h"
#include "ddgi_common.hlsl"

static DDGIVolume volume = ddgi_volume;

cbuffer Constants : register(b1)
{
	uint output_atlas_tex_id;
	uint ray_data_buffer_id;
	uint probes_to_update_buffer_id;
	uint metadata_atlas_tex_id;
};

static StructuredBuffer<uint> probes_to_update = ResourceDescriptorHeap[probes_to_update_buffer_id];

#ifdef IRRADIANCE
	[[vk::image_format("rgba16f")]]
	static RWTexture2DArray<float4> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];

	[[vk::image_format("rgba16f")]]
	static RWTexture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[metadata_atlas_tex_id];
#else
	[[vk::image_format("rg16f")]]
	static RWTexture2DArray<float2> output_atlas = ResourceDescriptorHeap[output_atlas_tex_id];
#endif

groupshared float4 ray_data_cache[TRACE_TEXELS * TRACE_TEXELS];

struct CSInput
{
	uint3 dispatch_thread_id : SV_DispatchThreadID;
	uint3 group_id : SV_GroupID;
	uint3 group_thread_id : SV_GroupThreadID;
	uint group_index : SV_GroupIndex;
};

// Copy border texels for correctly working hardware bilinear filtering (wrapping)
void WriteOctahedronBorder(uint3 texel_coord, uint2 oct_texel)
{
	uint3 source_coord = texel_coord;

	bool is_left_or_right = oct_texel.x == 0 || oct_texel.x == NUM_TEXELS - 1;
	bool is_top_or_bottom = oct_texel.y == 0 || oct_texel.y == NUM_TEXELS - 1;
	if (is_left_or_right && is_top_or_bottom)
	{
		source_coord.x += oct_texel.x == 0 ? NUM_TEXELS - 2 : -(NUM_TEXELS - 2);
		source_coord.y += oct_texel.y == 0 ? NUM_TEXELS - 2 : -(NUM_TEXELS - 2);
	} else if (is_left_or_right)
	{
		source_coord.x += oct_texel.x == 0 ? 1 : -1;
		source_coord.y += -oct_texel.y;
		source_coord.y += NUM_TEXELS - 1 - oct_texel.y;
	} else if (is_top_or_bottom)
	{
		source_coord.y += oct_texel.y == 0 ? 1 : -1;
		source_coord.x += -oct_texel.x;
		source_coord.x += NUM_TEXELS - 1 - oct_texel.x;
	}

	output_atlas[texel_coord] = output_atlas[source_coord];
}

[numthreads(NUM_TEXELS, NUM_TEXELS, 1)]
void CSMain(CSInput input)
{
	uint probe_id = probes_to_update[input.group_id.x];

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint cascade_id = GetProbeCascade(volume, probe_id);

	DDGICascade cascade = volume.cascades[cascade_id];

	uint3 texel_coord = GetProbeTexelCoords(volume, probe_coord, cascade_id, NUM_TEXELS, input.group_thread_id.xy);

	float2 oct_coord = GetNormalizedOctahedralCoordinates(int2(input.group_thread_id.xy) - 1, NUM_TEXELS - 2);
	float3 texel_direction = GetOctahedralDirection(oct_coord);

	StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[ray_data_buffer_id];
	for (uint cache_index = input.group_index; cache_index < volume.rays_per_probe; cache_index += NUM_TEXELS * NUM_TEXELS)
		ray_data_cache[cache_index] = ray_data_buffer[GetRayDataIndex(input.group_id.x, cache_index, volume)];
	GroupMemoryBarrierWithGroupSync();

	#ifdef IRRADIANCE
		// Irradiance is smooth. It is an integral over all rays around
		float3 irradiance_average = float3(0, 0, 0);
		float weight_sum = 0;
		float luminance_sum = 0;

		float max_back_faces = volume.rays_per_probe * BACKFACE_THRESHOLD;
		int back_face_count = 0;

		for (uint ray_index = 0; ray_index < volume.rays_per_probe; ray_index++)
		{
			float4 ray_data = ray_data_cache[ray_index];
			float3 radiance = ray_data.rgb;
			float distance = ray_data.a;

			if (distance < 0)
			{
				back_face_count++;
				if (back_face_count > max_back_faces)
					return;
				continue;
			}
			float3 ray_direction = GetProbeRayDirection(ray_index, volume);
			float weight = saturate(dot(ray_direction, texel_direction));

			irradiance_average += radiance * weight;
			weight_sum += weight;
			luminance_sum += Luminance(radiance);
		}

		// Store average luminance scale. This is used for encoding irradiance near 1.0
		float lighting_scale = max(luminance_sum / volume.rays_per_probe, 1e-6f);
		if (all(input.group_thread_id.xy == 0))
			metadata_atlas[uint3(GetProbeCandidateTexelCoords(volume, probe_coord), cascade_id)].w = lighting_scale;

		// Don't accumulate, use fixed rays
		// Normalize irradiance. To match the Monte Carlo Estimator of Irradiance. We have cosine weighted thats why factor of 2.0.
		irradiance_average /= (2.0 * max(weight_sum, 1e-9f)) * lighting_scale;
		output_atlas[texel_coord] = float4(irradiance_average, 1.0f);
	#else
		// Depth doensn't need to be smooth. It is better to be precise, so interpolate inside its texels (2x2 box filter in general)
		float max_ray_distance = length(cascade.spacing.xyz) * (1.0f + RELOCATION_MAX_OFFSET);
		int interior_texels = NUM_TEXELS - 2;
		uint ray_block_size = TRACE_TEXELS / interior_texels;
		uint2 interior_texel = clamp(int2(input.group_thread_id.xy) - 1, 0, interior_texels - 1);
		uint2 first_ray_texel = interior_texel * ray_block_size;

		float2 distance_average = float2(0, 0);
		for (uint y = 0; y < ray_block_size; y++)
		{
			for (uint x = 0; x < ray_block_size; x++)
			{
				uint2 ray_texel = first_ray_texel + uint2(x, y);
				float ray_distance = ray_data_cache[GetProbeRayIndex(ray_texel)].a;
				ray_distance = min(abs(ray_distance), max_ray_distance);
				distance_average += float2(ray_distance, ray_distance * ray_distance);
			}
		}

		// Don't accumulate, use fixed rays
		output_atlas[texel_coord] = distance_average / (ray_block_size * ray_block_size);
	#endif

    AllMemoryBarrierWithGroupSync();
	WriteOctahedronBorder(texel_coord, input.group_thread_id.xy);
}
