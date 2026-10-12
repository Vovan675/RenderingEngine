#include "../../common.h"

struct VSInput
{
	float2 uv : TEXCOORD0;
};

cbuffer Constants : register(b0)
{
	uint overlay_tex_id;
};

static Texture2D<float> confidence_texture = ResourceDescriptorHeap[overlay_tex_id];

// https://www.shadertoy.com/view/ls2Bz1
float3 ColorizeZucconi( float x )
{
	// Original solution converts visible wavelengths of light (400-700 nm) (represented as x = [0; 1]) to RGB colors
	x = saturate( x ) * 0.85;

	const float3 c1 = float3( 3.54585104, 2.93225262, 2.41593945 );
	const float3 x1 = float3( 0.69549072, 0.49228336, 0.27699880 );
	const float3 y1 = float3( 0.02312639, 0.15225084, 0.52607955 );

	float3 t = c1 * ( x - x1 );
	float3 a = saturate( 1.0 - t * t - y1 );

	const float3 c2 = float3( 3.90307140, 3.21182957, 3.96587128 );
	const float3 x2 = float3( 0.11748627, 0.86755042, 0.66077860 );
	const float3 y2 = float3( 0.84897130, 0.88445281, 0.73949448 );

	float3 k = c2 * ( x - x2 );
	float3 b = saturate( 1.0 - k * k - y2 );

	return saturate( a + b );
}

// Reference: RTXDI FullSample VisualizeConfidence
float4 PSMain(VSInput input) : SV_Target
{
	float value = saturate(1.0 - confidence_texture.SampleLevel(point_clamp_sampler, input.uv, 0));

	float4 result;
	result.a = pow(value, 0.5);
	result.rgb = ColorizeZucconi(value);

	return result;
}
