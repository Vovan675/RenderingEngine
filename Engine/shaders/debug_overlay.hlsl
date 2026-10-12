#include "common.h"

struct VSInput
{
	float2 uv : TEXCOORD0;
};

cbuffer Constants : register(b0)
{
	uint overlay_tex_id;
};

static Texture2D overlay_texture = ResourceDescriptorHeap[overlay_tex_id];

float4 PSMain(VSInput input) : SV_Target
{
	return overlay_texture.SampleLevel(linear_clamp_sampler, input.uv, 0);
}
