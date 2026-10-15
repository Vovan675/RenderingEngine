#include "../common.h"

struct VSInput
{
	float2 uv : TEXCOORD0;
};

cbuffer Constants : register(b0)
{
	uint src_tex_id;
};

static Texture2D src_texture = ResourceDescriptorHeap[src_tex_id];

float4 PSMain(VSInput input) : SV_Target
{
	return src_texture.SampleLevel(linear_clamp_sampler, input.uv, 0);
}
