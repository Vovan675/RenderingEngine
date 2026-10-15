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

float PSMain(VSInput input) : SV_Depth
{
	return src_texture.SampleLevel(point_clamp_sampler, input.uv, 0).r;
}
