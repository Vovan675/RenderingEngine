#include "../../common.h"
#include "rtxdi_gradients.h"

cbuffer Constants : register(b0)
{
	uint input_tex_id;
	uint output_tex_id;
	uint pass_index;
};

static Texture2D<float4> input_texture = ResourceDescriptorHeap[input_tex_id];
static RWTexture2D<float4> output_texture = ResourceDescriptorHeap[output_tex_id];

// Multipass A-trous blur of the gradients, the step doubles every pass for very wide kernel.
[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	int2 gradient_resolution = int2(getGradientResolution());
	if (any(pixel >= uint2(gradient_resolution)))
		return;

	int step = 1l << pass_index;

	float4 sum = 0.0;
	float weight_sum = 0.0;

	[unroll]
	for (int y = -1; y <= 1; y++)
	{
		[unroll]
		for (int x = -1; x <= 1; x++)
		{
			int2 position = int2(pixel) + int2(x, y) * step;
			if (all(position >= 0) && all(position < gradient_resolution))
			{
				// Triangular filter kernel produces a smooth result after a few iterations
				float weight = (x == 0 ? 1.0 : 0.5) * (y == 0 ? 1.0 : 0.5);
				
				sum += input_texture[position] * weight;
				weight_sum += weight;
			}
		}
	}

	output_texture[pixel] = sum / weight_sum;
}
