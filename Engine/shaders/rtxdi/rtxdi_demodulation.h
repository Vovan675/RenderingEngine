#pragma once

#include "../shading.h"
#include "NRD.hlsli"

void getDemodulationFactors(float3 normal, float3 view_dir, float3 albedo, float metalness, float roughness, float specular, out float3 diffuse_factor, out float3 specular_factor)
{
	roughness = max(saturate(roughness), MIN_PERCEPTUAL_ROUGHNESS);
	NRD_MaterialFactors(normal, view_dir, albedo * (1.0 - metalness), computeF0(albedo, metalness, specular), roughness, diffuse_factor, specular_factor);
}

float4 packNRDRadiance(float3 radiance, float hit_distance)
{
	return RELAX_FrontEnd_PackRadianceAndHitDist(radiance * camera_exposure, hit_distance, true);
}

float3 unpackNRDRadiance(float4 packed)
{
	return RELAX_BackEnd_UnpackRadiance(packed).rgb / camera_exposure;
}
