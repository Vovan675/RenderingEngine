#include "../common.h"
#include "../bindless.h"
#include "rtxdi_demodulation.h"

cbuffer Constants : register(b0)
{
	uint depth_tex_id;
	uint normal_tex_id;
	uint albedo_tex_id;
	uint shading_tex_id;
	uint denoised_diffuse_tex_id;
	uint denoised_specular_tex_id;
	uint diffuse_tex_id;
	uint specular_tex_id;
};

static Texture2D depth_texture = ResourceDescriptorHeap[depth_tex_id];
static Texture2D normal_texture = ResourceDescriptorHeap[normal_tex_id];
static Texture2D albedo_texture = ResourceDescriptorHeap[albedo_tex_id];
static Texture2D shading_texture = ResourceDescriptorHeap[shading_tex_id];
static Texture2D denoised_diffuse_texture = ResourceDescriptorHeap[denoised_diffuse_tex_id];
static Texture2D denoised_specular_texture = ResourceDescriptorHeap[denoised_specular_tex_id];
static RWTexture2D<float4> diffuse_texture = ResourceDescriptorHeap[diffuse_tex_id];
static RWTexture2D<float4> specular_texture = ResourceDescriptorHeap[specular_tex_id];

[numthreads(8, 8, 1)]
void CSMain(uint2 pixel : SV_DispatchThreadID)
{
	if (any(pixel >= uint2(render_resolution.xy)))
		return;

	float depth = depth_texture.Load(int3(pixel, 0)).r;
	if (depth <= 0.0)
	{
		diffuse_texture[pixel] = 0.0;
		specular_texture[pixel] = 0.0;
		return;
	}

	float2 uv = (pixel + 0.5) * render_resolution.zw;
	float3 world_position = GetWSPosition(uv, depth);
	float3 view_dir = normalize(camera_position.xyz - world_position);
	float3 normal = unpackGBufferNormal(normal_texture.Load(int3(pixel, 0)).rgb);
	float3 albedo = albedo_texture.Load(int3(pixel, 0)).rgb;
	float4 shading = shading_texture.Load(int3(pixel, 0));

	float3 diffuse_factor;
	float3 specular_factor;
	getDemodulationFactors(normal, view_dir, albedo, shading.r, shading.g, shading.b, diffuse_factor, specular_factor);

	float3 diffuse = unpackNRDRadiance(denoised_diffuse_texture.Load(int3(pixel, 0)));
	float3 specular = unpackNRDRadiance(denoised_specular_texture.Load(int3(pixel, 0)));
	diffuse_texture[pixel] = float4(diffuse * diffuse_factor, 0.0);
	specular_texture[pixel] = float4(specular * specular_factor, 0.0);
}
