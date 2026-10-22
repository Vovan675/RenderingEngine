cbuffer DDGIAtlases : register(b2)
{
	uint ddgi_distance_atlas_tex_id;
	uint ddgi_irradiance_atlas_tex_id;
	uint ddgi_metadata_atlas_tex_id;
};

#define DISTANCE_TEXELS 12
#define IRRADIANCE_TEXELS 8

#define MISS_DISTANCE 1e27f
#define BACKFACE_DISTANCE_SCALE 0.2
#define BACKFACE_THRESHOLD 0.25f

#define SURFACE_BIAS_NORMAL 0.075f
#define SURFACE_BIAS_CAMERA 0.15f
#define RELOCATION_MIN_DISTANCE 0.25f
#define RELOCATION_MAX_OFFSET 0.45f

// States for probes
#define STATE_ENABLED 0
#define STATE_DISABLED 1

uint GetRayDataIndex(uint update_index, uint ray_index, DDGIVolume volume)
{
	return update_index * volume.rays_per_probe + ray_index;
}

uint GetProbeCascade(DDGIVolume volume, uint probe_id)
{
	return probe_id / volume.size.w;
}

// Returns local coords in cascade
uint3 GetProbeGridCoords(DDGIVolume volume, uint probe_id)
{
	uint local_probe_id = probe_id % volume.size.w;
	uint3 coord;
	coord.x = local_probe_id % uint(volume.size.x);
	coord.y = local_probe_id / (uint(volume.size.x) * uint(volume.size.z));
	coord.z = (local_probe_id / uint(volume.size.x)) % uint(volume.size.z);
	return coord;
}

uint2 GetProbeStartTexelCoords(DDGIVolume volume, uint3 probe_coords)
{
	float layer_offset = volume.size.x * probe_coords.y;
	return uint2(probe_coords.x + layer_offset, probe_coords.z);
}

uint2 GetProbeCandidateTexelCoords(DDGIVolume volume, uint3 probe_coords)
{
	return GetProbeStartTexelCoords(volume, probe_coords) + uint2(0, volume.size.z);
}

uint3 GetProbeTexelCoords(DDGIVolume volume, uint3 probe_coords, uint cascade, uint num_texels, uint2 texel_offset)
{
	return uint3(GetProbeStartTexelCoords(volume, probe_coords) * num_texels + texel_offset, cascade);
}

float3 GetProbeWorldBasePosition(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	DDGICascade casc = volume.cascades[cascade];
	return casc.min.xyz + probe_coords * casc.spacing.xyz;
}

float3 GetProbeRelocationOffset(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[ddgi_metadata_atlas_tex_id];
	DDGICascade casc = volume.cascades[cascade];
	return metadata_atlas.Load(uint4(texel_coord.xy, cascade, 0)).xyz * casc.spacing.xyz;
}

// Returns world space offset
float3 GetProbeCandidateOffset(DDGIVolume volume, uint3 probe_coords, uint cascade, RWTexture2DArray<float4> metadata_atlas)
{
	uint2 texel_coord = GetProbeCandidateTexelCoords(volume, probe_coords);
	DDGICascade casc = volume.cascades[cascade];
	return metadata_atlas[uint3(texel_coord, cascade)].xyz * casc.spacing.xyz;
}

uint GetProbeState(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeStartTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[ddgi_metadata_atlas_tex_id];
	return metadata_atlas.Load(uint4(texel_coord.xy, cascade, 0)).a;
}

bool IsProbeDisabled(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	if (!volume.use_classification)
		return false;
	
	uint state = GetProbeState(volume, probe_coords, cascade);
	return state == STATE_DISABLED;
}

float GetProbeLightingScale(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeCandidateTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[ddgi_metadata_atlas_tex_id];
	return metadata_atlas.Load(uint4(texel_coord, cascade, 0)).w;
}

float3 GetProbeWorldPosition(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	float3 base_position = GetProbeWorldBasePosition(volume, probe_coords, cascade);
	if (volume.use_relocation)
	{
		float3 relocation_offset = GetProbeRelocationOffset(volume, probe_coords, cascade);
		base_position += relocation_offset;
	}
	return base_position;
}

float3 GetProbeCandidateOffset(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	uint2 texel_coord = GetProbeCandidateTexelCoords(volume, probe_coords);
	Texture2DArray<float4> metadata_atlas = ResourceDescriptorHeap[ddgi_metadata_atlas_tex_id];
	DDGICascade casc = volume.cascades[cascade];
	return metadata_atlas.Load(uint4(texel_coord.xy, cascade, 0)).xyz * casc.spacing.xyz;
}

// Relocation writes the new offset inside candidate.
// But sampling uses the current (old) offset the atlases were traced from
float3 GetProbeTracePosition(DDGIVolume volume, uint3 probe_coords, uint cascade)
{
	float3 base_position = GetProbeWorldBasePosition(volume, probe_coords, cascade);
	if (volume.use_relocation)
	{
		float3 candidate_offset = GetProbeCandidateOffset(volume, probe_coords, cascade);
		base_position += candidate_offset;
	}
	return base_position;
}

float2 GetNormalizedOctahedralCoordinates(int2 tex_coords, int num_texels)
{
	// Map 2D texture coordinates to a normalized octahedral space
	float2 oct_coord = float2(tex_coords.x % num_texels, tex_coords.y % num_texels);

	// Move to the center of a texel
	oct_coord.xy += 0.5f;

	// Normalize
	oct_coord.xy /= float(num_texels);

	// Shift to [-1, 1)
	oct_coord *= 2.f;
	oct_coord -= float2(1.f, 1.f);

	return oct_coord;
}

float SignNotZero(float v)
{
	return (v >= 0.f) ? 1.f : -1.f;
}
float2 SignNotZero(float2 v)
{
	return float2(SignNotZero(v.x), SignNotZero(v.y));
}

// Compute the normalized octahedral direction from [-1, 1] square
float3 GetOctahedralDirection(float2 coords)
{
	float3 direction = float3(coords.x, coords.y, 1.f - abs(coords.x) - abs(coords.y));
	if (direction.z < 0.f)
	{
		direction.xy = (1.f - abs(direction.yx)) * SignNotZero(direction.xy);
	}
	return normalize(direction);
}


// Compute the octant coordinates in the normalized [-1, 1] square, for unit direction vector.
float2 GetOctahedralCoordinates(float3 direction)
{
	float l1_norm = abs(direction.x) + abs(direction.y) + abs(direction.z);
	float2 uv = direction.xy * (1.f / l1_norm);
	if (direction.z < 0.f)
	{
		uv = (1.f - abs(uv.yx)) * SignNotZero(uv.xy);
	}
	return uv;
}

// Tracing result is 20x20 pixels
#define TRACE_TEXELS 20

float3 GetProbeRayDirection(uint ray_index, DDGIVolume volume)
{
	uint2 ray_texel = uint2(ray_index % TRACE_TEXELS, ray_index / TRACE_TEXELS);

	float2 oct_coord = GetNormalizedOctahedralCoordinates(ray_texel.xy, TRACE_TEXELS);
	float3 texel_direction = GetOctahedralDirection(oct_coord);
	return normalize(texel_direction);
}

uint GetProbeRayIndex(uint2 ray_texel)
{
	return ray_texel.y * TRACE_TEXELS + ray_texel.x;
}

uint GetProbeRayIndex(float3 direction)
{
	uint2 ray_texel = uint2(min((GetOctahedralCoordinates(direction) * 0.5f + 0.5f) * TRACE_TEXELS, TRACE_TEXELS - 1));
	return GetProbeRayIndex(ray_texel);
}

float3 GetCascadeCenter(DDGIVolume volume, DDGICascade cascade)
{
	return cascade.min.xyz + (volume.size.xyz - 1) * cascade.spacing.xyz * 0.5f;
}

float3 GetCascadeExtent(DDGIVolume volume, DDGICascade cascade)
{
	return (volume.size.xyz - 1) * 0.5f * cascade.spacing.xyz;
}

float GetVolumeWeight(float3 world_position, DDGIVolume volume)
{
	DDGICascade last_cascade = volume.cascades[volume.cascades_count - 1];
	float3 distance_to_edge = GetCascadeExtent(volume, last_cascade) - abs(world_position - GetCascadeCenter(volume, last_cascade));
	float3 edge_weight = saturate(distance_to_edge / last_cascade.spacing.xyz);
	return edge_weight.x * edge_weight.y * edge_weight.z;
}

float2 GetProbeUV(DDGIVolume volume, uint3 probe_coords, float3 direction, int texels)
{
	float texture_width = (volume.size.x * texels) * volume.size.y;
	float texture_height = volume.size.z * texels;
	float layer_offset = volume.size.x * texels * probe_coords.y;

	float2 start_uv = float2(probe_coords.x * texels + layer_offset, probe_coords.z * texels);
	start_uv += 1;
	float2 oct_coord = (GetOctahedralCoordinates(direction) * 0.5 + 0.5) * (texels - 2);
	start_uv += oct_coord;

	start_uv /= float2(texture_width, texture_height);
	return start_uv;
}

bool GetCascadeForPosition(DDGIVolume volume, out DDGICascade cascade, out uint cascade_index, float3 world_pos)
{
	// Find minimum cascade where position is inside
	for (int i = 0; i < volume.cascades_count; i++)
	{
		cascade = volume.cascades[i];
		if (all(abs(world_pos - GetCascadeCenter(volume, cascade)) < GetCascadeExtent(volume, cascade)))
		{
			cascade_index = i;
			return true;
		}
	}

	// If no cascade found return false and last cascade
	cascade_index = volume.cascades_count - 1;
	cascade = volume.cascades[cascade_index];
	return false;
}

float3 GetSurfaceBias(float3 surface_normal, float3 camera_position, float3 sample_position, DDGIVolume volume, uint cascade_id)
{
	DDGICascade cascade = volume.cascades[cascade_id];
	float3 to_camera = camera_position - sample_position;
	float to_camera_distance = length(to_camera);
	float3 to_camera_direction = to_camera / to_camera_distance;
	float3 normal_bias = surface_normal * SURFACE_BIAS_NORMAL;
	float3 camera_bias = to_camera_direction * SURFACE_BIAS_CAMERA * min(1.0f, to_camera_distance);
	return (normal_bias + camera_bias) * cascade.spacing.xyz;
}

float3 SampleIrradiance(float3 world_position, float3 world_normal, float3 surface_bias, DDGIVolume volume, uint cascade_id)
{
	DDGICascade cascade = volume.cascades[cascade_id];
	world_position += surface_bias;
	world_position = clamp(world_position, cascade.min.xyz, cascade.min.xyz + (volume.size.xyz - 1) * cascade.spacing.xyz);

	float3 sum_irradiance = 0;
	float sum_weight = 0;

	float3 relative_position = world_position - cascade.min.xyz;
	int3 base_probe_coords = int3(relative_position / cascade.spacing.xyz);
	base_probe_coords = clamp(base_probe_coords, int3(0, 0, 0), volume.size.xyz - 1);
	
	float3 base_probe_world_position = GetProbeWorldBasePosition(volume, base_probe_coords, cascade_id);
	// Alpha is how far from the floor(currentVertex) position. on [0, 1] for each axis.
	float3 alpha = saturate((world_position - base_probe_world_position) / cascade.spacing.xyz);

	for (int i = 0; i < 8; i++)
	{
		int3 offset = int3(i, i >> 1, i >> 2) & int3(1, 1, 1);
		
		int3 probe_coords = clamp(base_probe_coords + offset, int3(0, 0, 0), volume.size.xyz - 1);

		if (IsProbeDisabled(volume, probe_coords, cascade_id))
			continue;

		float2 probe_uv_irradiance = GetProbeUV(volume, probe_coords, world_normal, IRRADIANCE_TEXELS);
		float3 biased_to_probe = GetProbeWorldPosition(volume, probe_coords, cascade_id) - world_position;
		float3 biased_to_probe_direction = normalize(biased_to_probe);
		float2 probe_uv_distance = GetProbeUV(volume, probe_coords, -biased_to_probe_direction, DISTANCE_TEXELS);

		Texture2DArray irradiance_atlas = ResourceDescriptorHeap[ddgi_irradiance_atlas_tex_id];
		float3 irradiance = irradiance_atlas.SampleLevel(linear_clamp_sampler, float3(probe_uv_irradiance, cascade_id), 0).rgb;
		irradiance *= GetProbeLightingScale(volume, probe_coords, cascade_id);

		#if 0
			// This creates a sharp cutoff at the edge, visible when classification is used and probes are disabled
			float weight = saturate(dot(biased_to_probe_direction, world_normal));
		#elif 0
			// [Sloan et al. 11] “Wrap Shading”
			// Smooth transition (like NVIDIA DDGI)
			float wrap_shading = (dot(biased_to_probe_direction, world_normal) + 1.0f) * 0.5f;
			float weight = (wrap_shading * wrap_shading) + 0.2f;
		#else
			// Another way of smooth transition
			float wrap_shading = (dot(biased_to_probe_direction, world_normal) + 1.0f) * 0.5f;
			float weight = (wrap_shading * wrap_shading);
		#endif

		Texture2DArray distance_atlas = ResourceDescriptorHeap[ddgi_distance_atlas_tex_id];

		// Sample the probe's distance texture to get the mean distance to nearby surfaces
		float2 filtered_distance = distance_atlas.SampleLevel(linear_clamp_sampler, float3(probe_uv_distance, cascade_id), 0).rg;

		// Find the variance of the mean distance
		float variance = abs((filtered_distance.x * filtered_distance.x) - filtered_distance.y);

		float biased_to_probe_distance = length(biased_to_probe);
		// Occlusion test
		float chebyshev_weight = 1.f;
		if(biased_to_probe_distance > filtered_distance.x) // occluded
		{
			// v must be greater than 0, which is guaranteed by the if condition above.
			float v = biased_to_probe_distance - filtered_distance.x;
			chebyshev_weight = variance / (variance + (v * v));

			// Increase the contrast in the weight
			#if 0
				chebyshev_weight = max((chebyshev_weight * chebyshev_weight * chebyshev_weight), 0.f);
			#else
				chebyshev_weight = pow(chebyshev_weight, 6.0f);
			#endif
		}

		#if 0
			// Avoid visibility weights ever going all the way to zero because
			// when *no* probe has visibility we need a fallback value
			weight *= max(0.05f, chebyshev_weight);
		#else
			weight *= chebyshev_weight;
		#endif

		// Avoid a weight of zero
		weight = max(1e-6f, weight);

		// Crush tiny weights
		const float crush_threshold = 0.2f;
		if (weight < crush_threshold)
		{
			weight *= (weight * weight) * (1.f / (crush_threshold * crush_threshold));
		}

		float3 trilinear = max(0.001f, lerp(1.0 - alpha, alpha, offset));
		weight *= trilinear.x * trilinear.y * trilinear.z;

		sum_irradiance += irradiance * weight;
		sum_weight += weight;
	}

	if(sum_weight == 0.0f) return 0.0f;

	sum_irradiance /= sum_weight;
	sum_irradiance *= PI2;  // Multiply by the area of the integration domain (hemisphere) for Monte Carlo
	return sum_irradiance;
}

float GetCascadeWeight(float3 world_position, DDGIVolume volume, out uint cascade_index)
{
	DDGICascade cascade;
	GetCascadeForPosition(volume, cascade, cascade_index, world_position);
	if (cascade_index == volume.cascades_count - 1)
		return 1.0f;

	float3 distance_to_edge = GetCascadeExtent(volume, cascade) - abs(world_position - GetCascadeCenter(volume, cascade));
	return saturate(min(distance_to_edge.x, min(distance_to_edge.y, distance_to_edge.z)) / (cascade.spacing.x * 0.5f));
}

float3 SampleCascadesIrradiance(float3 world_position, float3 world_normal, float3 view_origin, DDGIVolume volume)
{
	uint cascade_index;
	float cascade_weight = GetCascadeWeight(world_position, volume, cascade_index);

	float3 irradiance = SampleIrradiance(world_position, world_normal, GetSurfaceBias(world_normal, view_origin, world_position, volume, cascade_index), volume, cascade_index) * cascade_weight;
	if (cascade_weight < 1.0f)
		irradiance += SampleIrradiance(world_position, world_normal, GetSurfaceBias(world_normal, view_origin, world_position, volume, cascade_index + 1), volume, cascade_index + 1) * (1.0f - cascade_weight);
	return irradiance;
}