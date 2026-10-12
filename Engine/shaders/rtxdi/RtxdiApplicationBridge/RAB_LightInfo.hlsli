#pragma once

#include "../../lighting/lighting.h"

#define RAB_LightInfo Light

RAB_LightInfo RAB_EmptyLightInfo()
{
	return (RAB_LightInfo)0;
}

RAB_LightInfo RAB_LoadLightInfo(uint index, bool prev_frame)
{
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[prev_frame ? old_lights_buffer_id : lights_buffer_id];
	return lights[index];
}

RAB_LightInfo RAB_LoadCompactLightInfo(uint linear_index)
{
	return RAB_EmptyLightInfo();
}

bool RAB_StoreCompactLightInfo(uint linear_index, RAB_LightInfo light_info)
{
	return false;
}

float RAB_GetLightTargetPdfForVolume(RAB_LightInfo light, float3 volume_center, float volume_radius)
{
	return 0.0;
}
