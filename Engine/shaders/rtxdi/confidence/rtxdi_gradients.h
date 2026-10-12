#pragma once

#define RTXDI_GRADIENT_FACTOR 3
#define RTXDI_GRADIENT_STORAGE_SCALE 256.0
#define RTXDI_GRADIENT_MAX_VALUE 65504.0

uint2 getGradientResolution()
{
	return (uint2(render_resolution.xy) + RTXDI_GRADIENT_FACTOR - 1) / RTXDI_GRADIENT_FACTOR;
}
