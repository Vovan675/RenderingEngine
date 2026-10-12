#pragma once

#define LINEAR_WRAP_SAMPLER_BINDLESS_ID 0
#define LINEAR_CLAMP_SAMPLER_BINDLESS_ID 1
#define POINT_WRAP_SAMPLER_BINDLESS_ID 2
#define POINT_CLAMP_SAMPLER_BINDLESS_ID 3
#define SHADOW_WRAP_SAMPLER_BINDLESS_ID 4
#define SHADOW_CLAMP_SAMPLER_BINDLESS_ID 5

// Must match in code
#ifndef RAY_TRACING_SHADER
static SamplerState linear_wrap_sampler = SamplerDescriptorHeap[LINEAR_WRAP_SAMPLER_BINDLESS_ID];
static SamplerState linear_clamp_sampler = SamplerDescriptorHeap[LINEAR_CLAMP_SAMPLER_BINDLESS_ID];
static SamplerState point_wrap_sampler = SamplerDescriptorHeap[POINT_WRAP_SAMPLER_BINDLESS_ID];
static SamplerState point_clamp_sampler = SamplerDescriptorHeap[POINT_CLAMP_SAMPLER_BINDLESS_ID];
static SamplerComparisonState shadow_wrap_sampler = SamplerDescriptorHeap[SHADOW_WRAP_SAMPLER_BINDLESS_ID];
static SamplerComparisonState shadow_clamp_sampler = SamplerDescriptorHeap[SHADOW_CLAMP_SAMPLER_BINDLESS_ID];
#else
#define linear_wrap_sampler ((SamplerState)SamplerDescriptorHeap[LINEAR_WRAP_SAMPLER_BINDLESS_ID])
#define linear_clamp_sampler ((SamplerState)SamplerDescriptorHeap[LINEAR_CLAMP_SAMPLER_BINDLESS_ID])
#define point_wrap_sampler ((SamplerState)SamplerDescriptorHeap[POINT_WRAP_SAMPLER_BINDLESS_ID])
#define point_clamp_sampler ((SamplerState)SamplerDescriptorHeap[POINT_CLAMP_SAMPLER_BINDLESS_ID])
#define shadow_wrap_sampler ((SamplerComparisonState)SamplerDescriptorHeap[SHADOW_WRAP_SAMPLER_BINDLESS_ID])
#define shadow_clamp_sampler ((SamplerComparisonState)SamplerDescriptorHeap[SHADOW_CLAMP_SAMPLER_BINDLESS_ID])
#endif

#ifndef RAY_TRACING_SHADER
float4 SampleTexture(uint index, float2 uv)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.Sample(linear_wrap_sampler, uv);
}

float4 SampleTexture(uint index, float2 uv, SamplerState sampler)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.Sample(sampler, uv);
}

float4 SampleTextureArray(uint index, float3 uvw, SamplerState sampler)
{
    Texture2DArray tex = ResourceDescriptorHeap[index];
    return tex.Sample(sampler, uvw);
}
#endif

float4 SampleTextureLevel(uint index, float2 uv, int lod)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.SampleLevel(linear_wrap_sampler, uv, lod);
}


float4 SampleTextureLevel(uint index, float2 uv, int lod, SamplerState sampler)
{
    Texture2D tex = ResourceDescriptorHeap[index];
    return tex.SampleLevel(sampler, uv, lod);
}