#pragma once

struct RAB_LightSample
{
	float3 position;
	float3 normal;
	float3 radiance;
	float solid_angle_pdf;
};

RAB_LightSample RAB_EmptyLightSample()
{
	return (RAB_LightSample)0;
}

bool RAB_IsAnalyticLightSample(RAB_LightSample light_sample)
{
	// TODO: support environment, emissive meshes
	return true;
}

float RAB_LightSampleSolidAnglePdf(RAB_LightSample light_sample)
{
	return light_sample.solid_angle_pdf;
}
