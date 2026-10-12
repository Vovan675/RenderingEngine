#pragma once

#include <Rtxdi/DI/Reservoir.hlsli>

bool shadeSurfaceWithLightSample(inout RTXDI_DIReservoir reservoir, RAB_Surface surface, RTXDI_ShadingParameters shading_params, RAB_LightSample light_sample,
	bool enable_visibility_reuse, bool enable_visibility_shortcut, out float3 diffuse, out float3 specular, out float light_distance)
{
	diffuse = 0;
	specular = 0;
	light_distance = 0;

	if (light_sample.solid_angle_pdf <= 0)
		return false;

	bool need_to_store = false;
	if (shading_params.enableFinalVisibility)
	{
		float3 visibility = 0;
		bool visibility_reused = false;

		if (shading_params.reuseFinalVisibility && enable_visibility_reuse)
		{
			RTXDI_VisibilityReuseParameters visibility_reuse_params;
			visibility_reuse_params.maxAge = shading_params.finalVisibilityMaxAge;
			visibility_reuse_params.maxDistance = shading_params.finalVisibilityMaxDistance;

			visibility_reused = RTXDI_GetDIReservoirVisibility(reservoir, visibility_reuse_params, visibility);
		}

		if (!visibility_reused)
		{
			visibility = RAB_GetConservativeVisibility(surface, light_sample) ? 1.0 : 0.0;
			RTXDI_StoreVisibilityInDIReservoir(reservoir, visibility, enable_visibility_shortcut);
			need_to_store = true;
		}

		light_sample.radiance *= visibility;
	}

	light_sample.radiance *= RTXDI_GetDIReservoirInvPdf(reservoir) / light_sample.solid_angle_pdf;

	if (any(light_sample.radiance > 0))
	{
		evaluateBrdf(surface, light_sample.position, diffuse, specular);
		diffuse *= light_sample.radiance;
		specular *= light_sample.radiance;

		light_distance = length(light_sample.position - surface.world_position);
	}

	return need_to_store;
}
