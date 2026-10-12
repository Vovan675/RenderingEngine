#include "../../common.h"
#include "../../bindless.h"
#include "rtxdi_gradients.h"
#include "../RtxdiApplicationBridge/RtxdiApplicationBridge.hlsli"
#include <Rtxdi/DI/ReservoirStorage.hlsli>
#include "../rtxdi_shading_helpers.h"

static Texture2D<int2> temporal_sample_positions_texture = ResourceDescriptorHeap[constants.temporal_sample_positions_tex_id];
static Texture2D<float2> luminance_texture = ResourceDescriptorHeap[constants.luminance_tex_id];
static Texture2D<float2> prev_luminance_texture = ResourceDescriptorHeap[constants.prev_luminance_tex_id];
static Texture2D motion_vectors_texture = ResourceDescriptorHeap[constants.motion_vectors_tex_id];
static RWTexture2D<float4> gradients_texture = ResourceDescriptorHeap[constants.gradients_tex_id];

// Reference: RTXDI FullSample GetPreviousWorldPos
float3 getPreviousWorldPosition(int2 window_position, float view_depth, float3 motion_vector)
{
	float2 prev_window_position = float2(window_position) + motion_vector.xy + 0.5;
	float prev_view_depth = view_depth + motion_vector.z;

	float2 uv = prev_window_position * render_resolution.zw;
	float3 view_direction = normalize(GetWSPosition(uv, 0.5, mul(old_iview, old_iprojection)) - old_camera_position.xyz);
	return old_camera_position.xyz + view_direction * prev_view_depth;
}

// Reference: RTXDI FullSample ComputeGradients.hlsl

[numthreads(8, 8, 1)]
void CSMain(uint2 stratum : SV_DispatchThreadID)
{
	if (any(stratum >= getGradientResolution()))
		return;

	// This shader runs one thread per image stratum, i.e. a square of pixels
	// (RTXDI_GRADIENT_FACTOR x RTXDI_GRADIENT_FACTOR) in size. One pixel is selected to
	// produce a gradient.

	bool use_prev_sample = false;
	int2 selected_pixel = -1;
	int2 selected_prev_pixel = -1;
	float2 selected_luminance = 0;

	// Iterate over all the pixels in the stratum, find one that is likely to produce
	// the brightest gradient. That means the pixel that has the brightest color coming
	// purely from light sampling in either the current or the previous frame.
	for (int y = 0; y < RTXDI_GRADIENT_FACTOR; y++)
	{
		for (int x = 0; x < RTXDI_GRADIENT_FACTOR; x++)
		{
			// Translate the gradient stratum index (stratum) into reservoir and pixel positions.
			int2 pixel = int2(stratum) * RTXDI_GRADIENT_FACTOR + int2(x, y);

			if (any(pixel >= int2(render_resolution.xy)))
				continue;

			// Find the matching pixel in the previous frame - that information is produced
			// by the temporal resampling or fused resampling shaders.
			int2 temporal_pixel = temporal_sample_positions_texture[pixel];

			// Load the previous frame sampled lighting luminance.
			// For invalid gradients, temporal_pixel is negative, and prev_luminance will be 0
			float2 prev_luminance = all(temporal_pixel >= 0) ? prev_luminance_texture[temporal_pixel] : 0;

			// Load the current frame sampled lighting luminance.
			float2 current_luminance = luminance_texture[pixel];

			float current_max = max(current_luminance.x, current_luminance.y);
			float prev_max = max(prev_luminance.x, prev_luminance.y);
			float selected_max = max(selected_luminance.x, selected_luminance.y);

			// Feed the brightest of (current, previous) samples into the gradient pixel selection logic.
			if (current_max > selected_max && current_max > prev_max)
			{
				use_prev_sample = false;
				selected_pixel = pixel;
				selected_prev_pixel = temporal_pixel;
				selected_luminance = current_luminance;
			} else if (prev_max > selected_max)
			{
				use_prev_sample = true;
				selected_pixel = pixel;
				selected_prev_pixel = temporal_pixel;
				selected_luminance = prev_luminance;
			}
		}
	}

	float4 gradient = 0;

	// If we have found at least one non-black pixel in the loop above...
	if (selected_luminance.x > 0 || selected_luminance.y > 0)
	{
		int2 selected_current_or_prev_pixel = use_prev_sample ? selected_prev_pixel : selected_pixel;

		// Load the reservoir that was selected for gradient evaluation, either from the current or the previous frame.
		RTXDI_DIReservoir selected_reservoir = RTXDI_LoadDIReservoir(constants.reservoir_params, selected_current_or_prev_pixel,
			use_prev_sample ? constants.buffer_indices.temporalResamplingInputBufferIndex : constants.buffer_indices.shadingInputBufferIndex);

		// Map the reservoir's light index into the other frame (previous or current)
		int selected_mapped_light_index = RAB_TranslateLightIndex(RTXDI_GetDIReservoirLightIndex(selected_reservoir), !use_prev_sample);

		if (selected_mapped_light_index >= 0)
		{
			// If the mapping was successful, compare the lighting.

			// Load the current G-buffer surface
			RAB_Surface surface = RAB_GetGBufferSurface(selected_pixel, false);

			// Find the world-space motion of the current surface
			float3 motion_vector = float3(motion_vectors_texture[selected_pixel].xy * render_resolution.xy, distance(surface.world_position, old_camera_position.xyz) - surface.view_depth);
			motion_vector.xy += jitter - old_jitter;

			float3 prev_world_position = getPreviousWorldPosition(selected_pixel, surface.view_depth, motion_vector);
			float3 world_motion = surface.world_position - prev_world_position;

			if (use_prev_sample)
			{
				// If the gradient is from the previous frame, use the previous surface for shading,
				// but reconstruct the new position using world_motion to avoid false-positive gradients
				// and self-occlusion from imperfectly matching G-buffer surfaces.
				surface = RAB_GetGBufferSurface(selected_prev_pixel, true);
				surface.world_position += world_motion;
			} else
			{
				// If the gradient is from the current frame, use the current surface for shading,
				// and reconstruct the previous world position using the motion vectors.
				surface.world_position = prev_world_position;
			}

			// Reconstruct the light sample
			RAB_LightInfo light_info = RAB_LoadLightInfo(selected_mapped_light_index, !use_prev_sample);
			RAB_LightSample light_sample = RAB_SamplePolymorphicLight(light_info, surface, RTXDI_GetDIReservoirSampleUV(selected_reservoir));

			// Shade the other (previous or current) surface using the other light sample
			float3 diffuse = 0;
			float3 specular = 0;
			float light_distance = 0;
			shadeSurfaceWithLightSample(selected_reservoir, surface, constants.shading_params, light_sample,
				/* enable_visibility_reuse = */ false, constants.temporal_params.enableVisibilityShortcut, diffuse, specular, light_distance);

			// Calculate the sampled lighting luminance for the other surface
			float2 new_luminance = float2(Luminance(diffuse), Luminance(specular)) * camera_exposure;

			// Convert to FP16 and back to avoid false-positive gradients due to precision loss in the
			// luminance_texture and prev_luminance_texture textures where selected_luminance comes from.
			new_luminance = f16tof32(f32tof16(new_luminance));

			// Compute the gradient
			gradient.xy = abs(selected_luminance - new_luminance);
			gradient.zw = max(selected_luminance, new_luminance);
		} else
		{
			// Light index mapping was unsuccessful, which means the light has either appeared or disappeared.
			// Which means the gradient is 100% of the pixel's luminance.
			gradient.xy = selected_luminance;
			gradient.zw = selected_luminance;
		}
	}

	// Store the output
	gradients_texture[stratum] = min(gradient * RTXDI_GRADIENT_STORAGE_SCALE, RTXDI_GRADIENT_MAX_VALUE);
}
