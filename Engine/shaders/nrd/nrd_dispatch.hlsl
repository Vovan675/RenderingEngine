#include "../bindless.h"
#define NRD_INTERNAL

cbuffer NRDResources : register(b1)
{
	uint4 nrd_input_ids[8];
	uint4 nrd_output_ids[4];
};

#define NRD_CONSTANTS_START(name) cbuffer name : register(b0) {
#define NRD_CONSTANT(type, name) type name;
#define NRD_CONSTANTS_END };

#define NRD_INPUTS_START
#define NRD_INPUT(resource_type, data_type, name, register_name, index) static resource_type<data_type> name = ResourceDescriptorHeap[nrd_input_ids[index / 4][index % 4]];
#define NRD_INPUTS_END

#define NRD_OUTPUTS_START
#define NRD_OUTPUT(resource_type, data_type, name, register_name, index) static resource_type<data_type> name = ResourceDescriptorHeap[nrd_output_ids[index / 4][index % 4]];
#define NRD_OUTPUTS_END

#define NRD_SAMPLERS_START
#define NRD_SAMPLER(resource_type, name, register_name, index) static resource_type name = SamplerDescriptorHeap[index == 0 ? POINT_CLAMP_SAMPLER_BINDLESS_ID : LINEAR_CLAMP_SAMPLER_BINDLESS_ID];
#define NRD_SAMPLERS_END

#include NRD_SHADER_FILE
