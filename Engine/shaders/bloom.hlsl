// Reference: Next Generation Post Processing in Call Of Duty
#include "common.h"
#include "bindless.h"

cbuffer Constants : register(b0)
{
	uint input_tex_id;
	uint output_tex_id;
	int2 output_size;
	float2 input_texel_size;
	float scatter;
	float threshold;
};

static const float MAX_BLOOM_VALUE = 65000.0;
static const float LUMINANCE_LIMIT = 50.0;
static const float BORDER_FADE_SIZE = 0.1;

static Texture2D input_texture = ResourceDescriptorHeap[input_tex_id];

[[vk::image_format("r11g11b10f")]]
static RWTexture2D<float3> output_texture = ResourceDescriptorHeap[output_tex_id];

// 13-tap downsample
float3 downsample13(float2 uv)
{
	float2 texel = input_texel_size;

	float3 a = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2, -2), 0).rgb;
	float3 b = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0, -2), 0).rgb;
	float3 c = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2, -2), 0).rgb;

	float3 d = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2,  0), 0).rgb;
	float3 e = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0,  0), 0).rgb;
	float3 f = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2,  0), 0).rgb;

	float3 g = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2,  2), 0).rgb;
	float3 h = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0,  2), 0).rgb;
	float3 i = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2,  2), 0).rgb;

	float3 j = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-1, -1), 0).rgb;
	float3 k = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 1, -1), 0).rgb;
	float3 l = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-1,  1), 0).rgb;
	float3 m = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 1,  1), 0).rgb;
	
	float3 res = e * 0.125;
	res += (a + c + g + i) * 0.03125;
	res += (b + d + f + h) * 0.0625;
	res += (j + k + l + m) * 0.125;
	return res;
	//return input_texture.SampleLevel(linear_clamp_sampler, uv, 0).rgb;
}

float karisWeight(float3 color)
{
	return 1.0f / (1.0f + Luminance(color) * camera_exposure);
}

// 13-tap downsample with karis average (average by luminance) per every box
float3 downsample13Karis(float2 uv)
{
	float2 texel = input_texel_size;

	float3 a = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2, -2), 0).rgb;
	float3 b = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0, -2), 0).rgb;
	float3 c = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2, -2), 0).rgb;

	float3 d = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2,  0), 0).rgb;
	float3 e = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0,  0), 0).rgb;
	float3 f = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2,  0), 0).rgb;

	float3 g = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-2,  2), 0).rgb;
	float3 h = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 0,  2), 0).rgb;
	float3 i = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 2,  2), 0).rgb;

	float3 j = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-1, -1), 0).rgb;
	float3 k = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 1, -1), 0).rgb;
	float3 l = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(-1,  1), 0).rgb;
	float3 m = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2( 1,  1), 0).rgb;
	
	float3 b0 = (a + b + d + e) * 0.25;
	float3 b1 = (b + c + e + f) * 0.25;
	float3 b2 = (d + e + g + h) * 0.25;
	float3 b3 = (e + f + h + i) * 0.25;
	float3 b4 = (j + k + l + m) * 0.25;

	float w0 = 0.125f * karisWeight(b0);
	float w1 = 0.125f * karisWeight(b1);
	float w2 = 0.125f * karisWeight(b2);
	float w3 = 0.125f * karisWeight(b3);
	float w4= 0.5f * karisWeight(b4);

	return (b0 * w0 + b1 * w1 + b2 * w2 + b3 * w3 + b4 * w4) / (w0 + w1 + w2 + w3 + w4);
	//return downsample13(uv);
}

// 3x3 tent filter
float3 upsampleTent(float2 uv)
{
	float2 texel = input_texel_size;

	float x0 = -1;
	float x1 = 0;
	float x2 = 1;

	float y0 = -1;
	float y1 = 0;
	float y2 = 1;

	float3 s00 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x0, y0), 0).rgb;
	float3 s10 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x1, y0), 0).rgb;
	float3 s20 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x2, y0), 0).rgb;
	float3 s01 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x0, y1), 0).rgb;
	float3 s11 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x1, y1), 0).rgb;
	float3 s21 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x2, y1), 0).rgb;
	float3 s02 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x0, y2), 0).rgb;
	float3 s12 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x1, y2), 0).rgb;
	float3 s22 = input_texture.SampleLevel(linear_clamp_sampler, uv + texel * float2(x2, y2), 0).rgb;

	return ((s00 + s20 + s02 + s22) + 2 * (s10 + s01 + s21 + s12) + 4 * s11) / 16.0f;
	//return input_texture.SampleLevel(linear_clamp_sampler, uv, 0).rgb;
}

float borderFade(float2 uv)
{
	float2 distance_to_border = 1.0 - abs(2.0 * uv - 1.0);
	return sqrt(saturate(min(distance_to_border.x, distance_to_border.y) / BORDER_FADE_SIZE));
}

[numthreads(8, 8, 1)]
void CSFirstDownsample(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(output_size)))
		return;

	float2 uv = (pixel + 0.5) / output_size;
	float3 color = downsample13Karis(uv) * camera_exposure; // scale by exposure to get near normalized values
	if (!all(isfinite(color)))
		color = 0;

	float luminance = Luminance(color);
	float limited_luminance = LUMINANCE_LIMIT * (1.0 - exp(-luminance / LUMINANCE_LIMIT));
	color *= limited_luminance / max(luminance, 1e-7);

	if (threshold > 0)
		color *= smoothstep(0, threshold, limited_luminance);

	output_texture[pixel] = clamp(color, 0, MAX_BLOOM_VALUE) * borderFade(uv);
}

[numthreads(8, 8, 1)]
void CSDownsample(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(output_size)))
		return;

	float2 uv = (pixel + 0.5) / output_size;
	output_texture[pixel] = downsample13(uv);
}

[numthreads(8, 8, 1)]
void CSUpsample(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(output_size)))
		return;

	float2 uv = (pixel + 0.5) / output_size;
	output_texture[pixel] = lerp(output_texture[pixel], upsampleTent(uv), scatter);
}
