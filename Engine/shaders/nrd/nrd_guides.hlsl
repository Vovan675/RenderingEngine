#include "../common.h"

// Fills data for https://github.com/NVIDIA-RTX/NRD/tree/v4.17.3#non-noisy-inputs

cbuffer Constants : register(b0)
{
	uint depth_tex_id;
	uint normal_tex_id;
	uint shading_tex_id;
	uint motion_vectors_tex_id;
	uint view_z_tex_id;
	uint normal_roughness_tex_id;
	uint motion_tex_id;
};

static Texture2D depth_texture = ResourceDescriptorHeap[depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
static Texture2D shading_texture = ResourceDescriptorHeap[shading_tex_id];
static Texture2D motion_vectors_texture = ResourceDescriptorHeap[motion_vectors_tex_id];
static RWTexture2D<float> view_z_texture = ResourceDescriptorHeap[view_z_tex_id];
static RWTexture2D<float4> normal_roughness_texture = ResourceDescriptorHeap[normal_roughness_tex_id];
static RWTexture2D<float4> motion_texture = ResourceDescriptorHeap[motion_tex_id];

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	float depth = depth_texture.Load(int3(pixel, 0)).r;
	float2 uv = (pixel + 0.5) * render_resolution.zw;
	float3 normal = unpackGBufferNormal(normal_texture.Load(int3(pixel, 0)).rgb);
	float roughness = shading_texture.Load(int3(pixel, 0)).g;
	float2 motion_uv = motion_vectors_texture.Load(int3(pixel, 0)).xy;

	float view_z = min(-GetVSPosition(uv, depth).z, z_far);
	float3 world_position = GetWSPosition(uv, depth);
	float prev_view_z = -mul(old_view, float4(world_position, 1.0)).z;

	view_z_texture[pixel] = view_z;
	normal_roughness_texture[pixel] = float4(normal * 0.5 + 0.5, roughness);
	motion_texture[pixel] = float4(motion_uv, prev_view_z - view_z, 0.0);
}
