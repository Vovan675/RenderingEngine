#include "../common.h"
#include "../bindless.h"
#include "rtxdi_demodulation.h"
#include "RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"

#include <Rtxdi/Utils/RandomSamplerPerPassSeeds.hlsli>
#include <Rtxdi/DI/InitialSampling.hlsli>
#include <Rtxdi/DI/ReservoirStorage.hlsli>
#include <Rtxdi/DI/TemporalResampling.hlsli>
#include <Rtxdi/DI/SpatialResampling.hlsli>
#include "rtxdi_shading_helpers.h"

#define RTXDI_GROUP_SIZE 8

static RWTexture2D<float4> diffuse_texture = ResourceDescriptorHeap[constants.diffuse_tex_id];
static RWTexture2D<float4> specular_texture = ResourceDescriptorHeap[constants.specular_tex_id];
#ifdef RTXDI_CONFIDENCE
static RWTexture2D<int2> temporal_sample_positions_texture = ResourceDescriptorHeap[constants.temporal_sample_positions_tex_id];
static RWTexture2D<float2> luminance_texture = ResourceDescriptorHeap[constants.luminance_tex_id];
#endif

[numthreads(RTXDI_GROUP_SIZE, RTXDI_GROUP_SIZE, 1)]
void InitialSamplingCS(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixel, constants.runtime_params.frameIndex, RTXDI_DI_GENERATE_INITIAL_SAMPLES_RANDOM_SEED);
	RTXDI_RandomSamplerState tile_rng = RTXDI_InitRandomSampler(pixel / RTXDI_TILE_SIZE_IN_PIXELS, constants.runtime_params.frameIndex, RTXDI_DI_GENERATE_INITIAL_SAMPLES_RANDOM_SEED);

	RAB_Surface surface = RAB_GetGBufferSurface(pixel, false);

	RAB_LightSample light_sample;
	RTXDI_DIReservoir reservoir = RTXDI_SampleLightsForSurface(rng, tile_rng, surface, constants.initial_sampling_params, constants.light_buffer_params, light_sample);

	RTXDI_StoreDIReservoir(reservoir, constants.reservoir_params, pixel, constants.buffer_indices.initialSamplingOutputBufferIndex);
}

[numthreads(RTXDI_GROUP_SIZE, RTXDI_GROUP_SIZE, 1)]
void TemporalResamplingCS(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixel, constants.runtime_params.frameIndex, RTXDI_DI_TEMPORAL_RESAMPLING_RANDOM_SEED);

	RAB_Surface surface = RAB_GetGBufferSurface(pixel, false);

	RTXDI_DIReservoir reservoir = RTXDI_EmptyDIReservoir();
	int2 temporal_sample_pixel = -1;
	if (RAB_IsSurfaceValid(surface))
	{
		RTXDI_DIReservoir initial_reservoir = RTXDI_LoadDIReservoir(constants.reservoir_params, pixel, constants.buffer_indices.initialSamplingOutputBufferIndex);

		Texture2D motion_vectors_texture = ResourceDescriptorHeap[constants.motion_vectors_tex_id];
		float2 motion_uv = motion_vectors_texture.Load(int3(pixel, 0)).xy;
		float prev_view_depth = distance(surface.world_position, old_camera_position.xyz);
		float3 motion = float3(motion_uv * render_resolution.xy, prev_view_depth - surface.view_depth);

		RAB_LightSample light_sample = RAB_EmptyLightSample();
		reservoir = RTXDI_DITemporalResampling(pixel, surface, initial_reservoir, rng, constants.runtime_params, constants.reservoir_params,
			motion, constants.buffer_indices.temporalResamplingInputBufferIndex, constants.temporal_params, temporal_sample_pixel, light_sample);
	}

#ifdef RTXDI_CONFIDENCE
	temporal_sample_positions_texture[pixel] = temporal_sample_pixel;
#endif

	RTXDI_StoreDIReservoir(reservoir, constants.reservoir_params, pixel, constants.buffer_indices.temporalResamplingOutputBufferIndex);
}

[numthreads(RTXDI_GROUP_SIZE, RTXDI_GROUP_SIZE, 1)]
void SpatialResamplingCS(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	RTXDI_RandomSamplerState rng = RTXDI_InitRandomSampler(pixel, constants.runtime_params.frameIndex, RTXDI_DI_SPATIAL_RESAMPLING_RANDOM_SEED);

	RAB_Surface surface = RAB_GetGBufferSurface(pixel, false);

	RTXDI_DIReservoir reservoir = RTXDI_EmptyDIReservoir();
	if (RAB_IsSurfaceValid(surface))
	{
		RTXDI_DIReservoir center_reservoir = RTXDI_LoadDIReservoir(constants.reservoir_params, pixel, constants.buffer_indices.spatialResamplingInputBufferIndex);

		RAB_LightSample light_sample = RAB_EmptyLightSample();
		reservoir = RTXDI_DISpatialResampling(pixel, surface, center_reservoir, rng, constants.runtime_params, constants.reservoir_params,
			constants.buffer_indices.spatialResamplingInputBufferIndex, constants.spatial_params, light_sample);
	}

	RTXDI_StoreDIReservoir(reservoir, constants.reservoir_params, pixel, constants.buffer_indices.spatialResamplingOutputBufferIndex);
}

[numthreads(RTXDI_GROUP_SIZE, RTXDI_GROUP_SIZE, 1)]
void ShadingCS(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	RAB_Surface surface = RAB_GetGBufferSurface(pixel, false);
	RTXDI_DIReservoir reservoir = RTXDI_LoadDIReservoir(constants.reservoir_params, pixel, constants.buffer_indices.shadingInputBufferIndex);

	float3 diffuse = 0;
	float3 specular = 0;
	float light_distance = 0;
	float2 current_luminance = 0;

	if (RTXDI_IsValidDIReservoir(reservoir))
	{
		RAB_LightInfo light_info = RAB_LoadLightInfo(RTXDI_GetDIReservoirLightIndex(reservoir), false);
		RAB_LightSample light_sample = RAB_SamplePolymorphicLight(light_info, surface, RTXDI_GetDIReservoirSampleUV(reservoir));

		bool need_to_store = shadeSurfaceWithLightSample(reservoir, surface, constants.shading_params, light_sample,
			true, constants.temporal_params.enableVisibilityShortcut, diffuse, specular, light_distance);

		current_luminance = float2(Luminance(diffuse), Luminance(specular)) * camera_exposure;

		if (need_to_store)
			RTXDI_StoreDIReservoir(reservoir, constants.reservoir_params, pixel, constants.buffer_indices.shadingInputBufferIndex);
	}

#ifdef RTXDI_CONFIDENCE
	// Store the sampled lighting luminance for the gradient pass.
	// Discard the pixels where the visibility was reused, as gradients need actual visibility.
	luminance_texture[pixel] = current_luminance * (reservoir.age > 0 ? 0 : 1);
#endif

#ifdef RTXDI_DENOISER
	const float contact_hit_distance = 0.001;
	float hit_distance = max(light_distance, contact_hit_distance);
	RAB_Material material = RAB_GetMaterial(surface);
	float3 diffuse_factor;
	float3 specular_factor;
	getDemodulationFactors(surface.normal, surface.view_direction, material.albedo, material.metalness, material.roughness, material.specular, diffuse_factor, specular_factor);
	diffuse_texture[pixel] = packNRDRadiance(diffuse / diffuse_factor, hit_distance);
	specular_texture[pixel] = packNRDRadiance(specular / specular_factor, hit_distance);
#else
	diffuse_texture[pixel] = float4(diffuse, 0.0);
	specular_texture[pixel] = float4(specular, 0.0);
#endif
}
