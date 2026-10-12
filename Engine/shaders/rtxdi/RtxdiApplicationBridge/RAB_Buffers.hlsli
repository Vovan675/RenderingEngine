#pragma once

#define RTXDI_ENABLE_PRESAMPLING 0
#include "Rtxdi/DI/ReSTIRDIParameters.h"

struct RTXDIConstants
{
	RTXDI_RuntimeParameters runtime_params;
	RTXDI_LightBufferParameters light_buffer_params;
	RTXDI_ReservoirBufferParameters reservoir_params;
	RTXDI_DIBufferIndices buffer_indices;
	RTXDI_DIInitialSamplingParameters initial_sampling_params;
	RTXDI_DITemporalResamplingParameters temporal_params;
	RTXDI_DISpatialResamplingParameters spatial_params;
	RTXDI_ShadingParameters shading_params;
	uint32_t depth_tex_id;
	uint32_t normal_tex_id;
	uint32_t albedo_tex_id;
	uint32_t shading_tex_id;
	uint32_t prev_depth_tex_id;
	uint32_t prev_normal_tex_id;
	uint32_t prev_albedo_tex_id;
	uint32_t prev_shading_tex_id;
	uint32_t motion_vectors_tex_id;
	uint32_t diffuse_tex_id;
	uint32_t specular_tex_id;
	uint32_t reservoirs_buffer_id;
	uint32_t neighbor_offsets_buffer_id;
	uint32_t temporal_sample_positions_tex_id;
	uint32_t luminance_tex_id;
	uint32_t prev_luminance_tex_id;
	uint32_t gradients_tex_id;
};

cbuffer Constants : register(b0)
{
	RTXDIConstants constants;
};

static RWStructuredBuffer<RTXDI_PackedDIReservoir> light_reservoirs = ResourceDescriptorHeap[constants.reservoirs_buffer_id];
static StructuredBuffer<float2> neighbor_offsets = ResourceDescriptorHeap[constants.neighbor_offsets_buffer_id];
static StructuredBuffer<uint> light_index_mapping = ResourceDescriptorHeap[light_index_mapping_buffer_id];

#define RTXDI_LIGHT_RESERVOIR_BUFFER light_reservoirs
#define RTXDI_NEIGHBOR_OFFSETS_BUFFER neighbor_offsets

int RAB_TranslateLightIndex(uint light_index, bool current_to_previous)
{
	return int(light_index_mapping[current_to_previous ? light_index : lights_count + light_index]);
}
