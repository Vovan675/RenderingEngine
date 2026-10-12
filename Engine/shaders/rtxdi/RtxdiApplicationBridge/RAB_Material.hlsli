#pragma once

struct RAB_Material
{
	float3 albedo;
	float metalness;
	float roughness;
	float specular;
};

RAB_Material RAB_EmptyMaterial()
{
	return (RAB_Material)0;
}

float3 getDiffuseAlbedo(RAB_Material material)
{
	return material.albedo * (1.0 - material.metalness);
}

float3 getSpecularF0(RAB_Material material)
{
	return computeF0(material.albedo, material.metalness, material.specular);
}

RAB_Material RAB_GetGBufferMaterial(int2 pixel_position, bool prev_frame)
{
	RAB_Material material = RAB_EmptyMaterial();

	if (any(pixel_position >= int2(render_resolution.xy)))
		return material;

	Texture2D albedo_texture = ResourceDescriptorHeap[prev_frame ? constants.prev_albedo_tex_id : constants.albedo_tex_id];
	Texture2D shading_texture = ResourceDescriptorHeap[prev_frame ? constants.prev_shading_tex_id : constants.shading_tex_id];

	float4 shading = shading_texture.Load(int3(pixel_position, 0));
	material.albedo = albedo_texture.Load(int3(pixel_position, 0)).rgb;
	material.metalness = shading.r;
	material.roughness = max(saturate(shading.g), MIN_PERCEPTUAL_ROUGHNESS);
	material.specular = shading.b;

	return material;
}

bool RAB_AreMaterialsSimilar(RAB_Material material, RAB_Material other_material)
{
	return true;
}
