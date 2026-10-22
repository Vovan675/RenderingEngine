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

struct CSInput
{
	uint3 dispatch_thread_id : SV_DispatchThreadID;
	uint3 group_id : SV_GroupID;
	uint3 group_thread_id : SV_GroupThreadID;
	uint group_index : SV_GroupIndex;
};

[numthreads(32, 1, 1)]
void CS_Relocate(CSInput input)
{
	if (input.dispatch_thread_id.x >= probes_to_update_count)
		return;

	uint probe_id = probes_to_update[input.dispatch_thread_id.x];
	uint cascade_id = GetProbeCascade(volume, probe_id);

	DDGICascade cascade = volume.cascades[cascade_id];

	int backface_count = 0;
	int closest_back_face_index = -1;
	int closest_front_face_index = -1;
	int farthest_front_face_index = -1;
	float closest_back_face_distance = 1e27f;
	float closest_front_face_distance = 1e27f;
	float farthest_front_face_distance = 0.0;

	uint rays_count = volume.rays_per_probe;

	StructuredBuffer<float4> ray_data_buffer = ResourceDescriptorHeap[ray_data_buffer_id];
	for (uint ray_index = 0; ray_index < rays_count; ray_index++)
	{
		uint ray_data_index = GetRayDataIndex(input.dispatch_thread_id.x, ray_index, volume);

		float ray_distance = ray_data_buffer[ray_data_index].a;
		if (ray_distance < 0)
		{
			backface_count++;

			// Invert distance to get the actual distance to the backface. And scale it to the actual distance.
			ray_distance = -ray_distance / BACKFACE_DISTANCE_SCALE;

			if (ray_distance < closest_back_face_distance)
			{
				closest_back_face_distance = ray_distance;
				closest_back_face_index = ray_index;
			}
		} else
		{
			if (ray_distance < closest_front_face_distance)
			{
				closest_front_face_distance = ray_distance;
				closest_front_face_index = ray_index;
			}
			if (ray_distance > farthest_front_face_distance && ray_distance < MISS_DISTANCE)
			{
				farthest_front_face_distance = ray_distance;
				farthest_front_face_index = ray_index;
			}
		}
	}

	uint3 probe_coord = GetProbeGridCoords(volume, probe_id);
	uint3 texel_coord = uint3(GetProbeStartTexelCoords(volume, probe_coord), cascade_id);
	uint3 candidate_texel_coord = uint3(GetProbeCandidateTexelCoords(volume, probe_coord), cascade_id);
	float3 offset = GetProbeCandidateOffset(volume, probe_coord, cascade_id, output_atlas);

	// Candidate (Traced) offset becomes current
	output_atlas[texel_coord].xyz = offset / cascade.spacing.xyz;

	// Now calculate new candidate offset for next frame
	float minimum_front_face_distance = RELOCATION_MIN_DISTANCE * min(cascade.spacing.x, min(cascade.spacing.y, cascade.spacing.z));
	float safe_front_face_distance = minimum_front_face_distance * 1.25f;

	float3 new_offset = 1000;
	// If there's a close backface AND you see more than % backfaces, assume you're inside something.
	bool is_too_much_backfaces = (float(backface_count) / float(rays_count)) > BACKFACE_THRESHOLD;
	if (closest_back_face_index != -1 && is_too_much_backfaces) {
		float3 closest_back_face_direction = normalize(GetProbeRayDirection(closest_back_face_index, volume));

		float move_distance = closest_back_face_distance + minimum_front_face_distance;
		if (dot(offset, closest_back_face_direction) >= 0.0f) // Don't offset back
			new_offset = offset + closest_back_face_direction * move_distance;
	} else if (closest_front_face_distance < minimum_front_face_distance)
	{
		// If no backface, and closest front face is too close, move away from it if there is space behind
		float3 closest_front_face_direction = GetProbeRayDirection(closest_front_face_index, volume);
		float3 farthest_front_face_direction = GetProbeRayDirection(farthest_front_face_index, volume);

		float3 away_direction = -closest_front_face_direction;
		uint away_ray_index = GetProbeRayIndex(away_direction);
		float away_distance = ray_data_buffer[GetRayDataIndex(input.dispatch_thread_id.x, away_ray_index, volume)].a;
		float missing_distance = minimum_front_face_distance - closest_front_face_distance;

		if (away_distance > safe_front_face_distance + missing_distance && dot(offset, away_direction) >= 0.0f)
		{
			// Space behind is free, step back just enough to reach the minimum distance
			new_offset = offset + away_direction * missing_distance;
		} else if (dot(closest_front_face_direction, farthest_front_face_direction) <= 0.0f && dot(offset, farthest_front_face_direction) >= 0.0f)
		{
			// If closest and farthest are not in the same direction, move towards farthest
			float move_distance = max(farthest_front_face_distance - safe_front_face_distance, 0.0f);
			new_offset = offset + farthest_front_face_direction * move_distance;
		}
	} else if (closest_front_face_distance > safe_front_face_distance && any(offset != 0.0f))
	{
		// If no backface, and closest front face is far enough, move towards zero offset
		float move_distance = min(length(offset), closest_front_face_distance - minimum_front_face_distance);
		float3 move_back_direction = normalize(-offset);
		new_offset = offset + move_back_direction * move_distance;
	}


	// Maximum offset is 45% of the spacing
	float max_offset = RELOCATION_MAX_OFFSET;
	float3 new_offset_norm = new_offset / cascade.spacing.xyz;
	if (length(new_offset_norm) < max_offset)
	{
		offset = new_offset;
	}

	output_atlas[candidate_texel_coord].xyz = float3(offset / cascade.spacing.xyz);
}
