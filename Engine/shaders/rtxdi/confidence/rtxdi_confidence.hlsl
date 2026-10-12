#include "../../common.h"
#include "rtxdi_gradients.h"

cbuffer Constants : register(b0)
{
	uint gradients_tex_id;
	uint motion_vectors_tex_id;
	uint prev_diffuse_confidence_tex_id;
	uint prev_specular_confidence_tex_id;
	uint diffuse_confidence_tex_id;
	uint specular_confidence_tex_id;
	float darkness_bias;
	float sensitivity;
	float blend_factor;
};

static Texture2D<float4> gradients_texture = ResourceDescriptorHeap[gradients_tex_id];
static Texture2D motion_vectors_texture = ResourceDescriptorHeap[motion_vectors_tex_id];
static Texture2D<float> prev_diffuse_confidence_texture = ResourceDescriptorHeap[prev_diffuse_confidence_tex_id];
static Texture2D<float> prev_specular_confidence_texture = ResourceDescriptorHeap[prev_specular_confidence_tex_id];
static RWTexture2D<float> diffuse_confidence_texture = ResourceDescriptorHeap[diffuse_confidence_tex_id];
static RWTexture2D<float> specular_confidence_texture = ResourceDescriptorHeap[specular_confidence_tex_id];

// Reference: RTXDI FullSample ConfidencePass.hlsl

// This shader implements the conversion of the filtered gradients into
// the confidence channel suitable for NRD consumption, and applies an exponential
// temporal filter on top of that confidence channel. Typically, the temporal filter
// has very short history, like 1 frame or less, just to reduce the flicker.

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	// Convert the output pixel position into UV in the gradients texture.
	float2 input_position = (float2(pixel) + 0.5) / RTXDI_GRADIENT_FACTOR;
	input_position /= getGradientResolution();

	// Sample the gradients texture with linear interpolation.
	float4 gradient = gradients_texture.SampleLevel(linear_clamp_sampler, input_position, 0);
	gradient = max(gradient, 0);

	// Apply the "darkness bias" to avoid discarding history because of noise on very dark surfaces.
	gradient.zw += darkness_bias * RTXDI_GRADIENT_STORAGE_SCALE;

	// Convert gradients to confidence.
	float diffuse_confidence = saturate(1.0 - gradient.x / gradient.z);
	float specular_confidence = saturate(1.0 - gradient.y / gradient.w);

	diffuse_confidence = saturate(pow(diffuse_confidence, sensitivity));
	specular_confidence = saturate(pow(specular_confidence, sensitivity));

	// Apply the temporal filter, if enabled.
	if (blend_factor < 1.0)
	{
		// Find the previous input position using the motion vector.
		float2 motion_vector = motion_vectors_texture[pixel].xy * render_resolution.xy;

		int2 prev_input_position = int2(float2(pixel) + 0.5 + motion_vector);

		if (all(prev_input_position >= 0) && all(prev_input_position < int2(render_resolution.xy)))
		{
			// Blend the history in a non-linear space to make the result
			// hold on to lower confidence values longer than to high confidence.
			// We want to discard history for more than one frame when a singular change happens
			// (e.g. a light turning off), because ReSTIR needs a little time to stabilize.
			const float power = 0.25;

			// Load the previous confidence values
			float prev_diffuse_confidence = prev_diffuse_confidence_texture[prev_input_position];
			float prev_specular_confidence = prev_specular_confidence_texture[prev_input_position];

			// Convert the current and previous values to the non-linear space
			diffuse_confidence = pow(diffuse_confidence, power);
			specular_confidence = pow(specular_confidence, power);
			prev_diffuse_confidence = pow(prev_diffuse_confidence, power);
			prev_specular_confidence = pow(prev_specular_confidence, power);

			// Blend
			diffuse_confidence = lerp(prev_diffuse_confidence, diffuse_confidence, blend_factor);
			specular_confidence = lerp(prev_specular_confidence, specular_confidence, blend_factor);

			// Convert the result back into linear space
			diffuse_confidence = pow(saturate(diffuse_confidence), 1.0 / power);
			specular_confidence = pow(saturate(specular_confidence), 1.0 / power);
		}
	}

	// Store the output
	diffuse_confidence_texture[pixel] = diffuse_confidence;
	specular_confidence_texture[pixel] = specular_confidence;
}
