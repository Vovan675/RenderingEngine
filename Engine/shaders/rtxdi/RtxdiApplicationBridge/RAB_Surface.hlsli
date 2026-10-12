#pragma once

#include "../../sampling.h"

#include "Rtxdi/Utils/RandomSamplerState.hlsli"
#include "RAB_Material.hlsli"

#define BACKGROUND_DEPTH 0.0f

struct RAB_Surface
{
	float3 world_position;
	float3 view_direction;
	float3 normal;
	float3 geometry_normal;
	float view_depth;
	float diffuse_probability;
	RAB_Material material;
};

RAB_Surface RAB_EmptySurface()
{
	RAB_Surface surface = (RAB_Surface)0;
	surface.view_depth = BACKGROUND_DEPTH;
	return surface;
}

bool RAB_IsSurfaceValid(RAB_Surface surface)
{
	return surface.view_depth != BACKGROUND_DEPTH;
}

float3 RAB_GetSurfaceWorldPos(RAB_Surface surface)
{
	return surface.world_position;
}

RAB_Material RAB_GetMaterial(RAB_Surface surface)
{
	return surface.material;
}

float3 RAB_GetSurfaceNormal(RAB_Surface surface)
{
	return surface.normal;
}

float RAB_GetSurfaceLinearDepth(RAB_Surface surface)
{
	return surface.view_depth;
}

float getSurfaceDiffuseProbability(RAB_Surface surface)
{
	float diffuse_weight = Luminance(getDiffuseAlbedo(surface.material));
	float specular_weight = Luminance(FresnelSchlick(getSpecularF0(surface.material), float3(1.0, 1.0, 1.0), dot(surface.view_direction, surface.normal)));
	float weights_sum = diffuse_weight + specular_weight;
	return weights_sum < 1e-7 ? 1.0 : diffuse_weight / weights_sum;
}

RAB_Surface RAB_GetGBufferSurface(int2 pixel_position, bool prev_frame)
{
	RAB_Surface surface = RAB_EmptySurface();

	if (any(pixel_position >= int2(render_resolution.xy)))
		return surface;

	Texture2D depth_texture = ResourceDescriptorHeap[prev_frame ? constants.prev_depth_tex_id : constants.depth_tex_id];
	float depth = depth_texture.Load(int3(pixel_position, 0)).r;

	if (depth <= BACKGROUND_DEPTH)
		return surface;

	surface.material = RAB_GetGBufferMaterial(pixel_position, prev_frame);

	Texture2D normal_texture = ResourceDescriptorHeap[prev_frame ? constants.prev_normal_tex_id : constants.normal_tex_id];
	float2 uv = (pixel_position + 0.5) * render_resolution.zw;
	float3 eye_position = prev_frame ? old_camera_position.xyz : camera_position.xyz;
	surface.world_position = prev_frame ? GetWSPosition(uv, depth, mul(old_iview, old_iprojection)) : GetWSPosition(uv, depth);
	surface.view_depth = distance(surface.world_position, eye_position);
	surface.normal = unpackGBufferNormal(normal_texture.Load(int3(pixel_position, 0)).rgb);
	surface.geometry_normal = surface.normal;
	surface.view_direction = normalize(eye_position - surface.world_position);
	surface.diffuse_probability = getSurfaceDiffuseProbability(surface);

	return surface;
}

bool RAB_SurfaceImportanceSampleBrdf(RAB_Surface surface, inout RTXDI_RandomSamplerState rng, out float3 direction)
{
	float3 rand;
	rand.x = RTXDI_GetNextRandom(rng);
	rand.y = RTXDI_GetNextRandom(rng);
	rand.z = RTXDI_GetNextRandom(rng);

	float3 tangent;
	float3 bitangent;
	BuildOrthonormalBasis(surface.normal, tangent, bitangent);

	if (rand.x < surface.diffuse_probability)
	{
		direction = TangentToWorld(SampleCosineHemisphere(rand.yz), tangent, bitangent, surface.normal);
	} else
	{
		float3 h = SampleGGXTangent(rand.yz, max(surface.material.roughness, MIN_PERCEPTUAL_ROUGHNESS));
		h = normalize(h);
		direction = reflect(-surface.view_direction, TangentToWorld(h, tangent, bitangent, surface.normal));
	}

	return dot(surface.normal, direction) > 0.0;
}

float RAB_SurfaceEvaluateBrdfPdf(RAB_Surface surface, float3 direction)
{
	float cos_theta = saturate(dot(surface.normal, direction));
	float diffuse_pdf = cos_theta / PI;
	float3 h = normalize(surface.view_direction + direction);
	float specular_pdf = GGX_PDF(saturate(dot(surface.normal, h)), saturate(dot(surface.view_direction, h)), max(surface.material.roughness, MIN_PERCEPTUAL_ROUGHNESS));
	float pdf = cos_theta > 0.0 ? lerp(specular_pdf, diffuse_pdf, surface.diffuse_probability) : 0.0;
	return pdf;
}
