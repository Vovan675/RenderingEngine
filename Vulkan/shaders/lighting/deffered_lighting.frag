#include "common.h"

layout(location = 0) in vec4 inPos;

layout (binding = 0) uniform UBO 
{
	mat4 model;
	mat4 light_matrix[4];
	vec4 cascade_splits;
} ubo;

layout (set=1, binding=0) uniform sampler2D textures[];

#if RAY_TRACED_SHADOWS == 1
	layout (set=0, binding=2) uniform sampler2D shadow_map;
#else
	#if LIGHT_TYPE == 0
		layout (set=0, binding=2) uniform samplerCube shadow_map;
	#else
		layout (set=0, binding=2) uniform sampler2DArray shadow_map;
	#endif
#endif

layout(set=0, binding=1) uniform UBOTextures
{
	uint albedoTexId;
	uint normalTexId;
	uint depthTexId;
	uint shadingTexId;
	uint shadowMapTexId;
};

layout(push_constant) uniform constants
{
	vec4 light_pos; // w == 1 for dir light
	vec4 light_color;
	float light_intensity;
	float light_range_square; // radius ^ 2
	float z_far;
	float padding[1];
} PushConstants;

layout(location = 0) out vec3 outDiffuse;
layout(location = 1) out vec3 outSpecular;

// F - Fresnel Schlick
vec3 F_Schlick(in vec3 f0, in float f90, in float u)
{
	return f0 + (f90 - f0) * pow(1.0f - u, 5.0f);
}

// D
float D_GGX(float NdotH, float a2)
{
	float f = (NdotH * a2 - NdotH) * NdotH + 1.0f;
	return a2 / (PI * f * f);
}

// V
float V_SmithGGXCorrelated(float NdotV, float NdotL, float roughness)
{
	float alphaRoughnessSq = roughness * roughness;

	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);

	float GGX = GGXV + GGXL;
	if (GGX > 0.0)
	{
		return 0.5 / GGX;
	}
	return 0.0;
}


// Diffuse
float Fr_DisneyDiffuse(float NdotV, float NdotL, float LdotH, float linearRoughness)
{
	float energyBias = mix(0, 0.5, linearRoughness);
	float energyFactor = mix(1.0, 1.0 / 1.51, linearRoughness);
	float fd90 = energyBias + 2.0 * LdotH * LdotH * linearRoughness;
	vec3 f0 = vec3(1.0f, 1.0f, 1.0f);
	float lightScatter = F_Schlick(f0, fd90, NdotL).r;
	float viewScatter = F_Schlick(f0, fd90, NdotV).r;
	return lightScatter * viewScatter * energyFactor;
}

float compute_reflectance(float reflectance)
{
	// Compute reflectance for dielectric (0.04 for reflectance = 0.5)
	return 0.16 * reflectance * reflectance;
}

// Moving Frostbite to pbr
float get_smooth_distance_att(float sqr_distance, float inv_sqr_att_radius)
{
	float factor = sqr_distance * (1 / inv_sqr_att_radius);
	float smooth_factor = clamp(1.0 - factor * factor, 0.0, 1.0);
	return smooth_factor * smooth_factor;
}

float get_attenuation(vec3 pos)
{
	vec3 delta = PushConstants.light_pos.xyz - pos;
	float sqr_distance = dot(delta, delta);
	float attenuation = 1.0 / max(sqr_distance, 0.01 * 0.01);
	attenuation *= get_smooth_distance_att(sqr_distance, PushConstants.light_range_square);
	return attenuation;
}

vec3 sampling_offsets[20] = vec3[]
(
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);   
#define SHADOW_MAP_CASCADE_COUNT 4

#if RAY_TRACED_SHADOWS == 0
	#if LIGHT_TYPE == 0
		float get_shadow_point(vec3 frag_pos, float bias)
		{
			vec3 fragToLight = frag_pos - PushConstants.light_pos.xyz;
			float current_depth = length(fragToLight) / PushConstants.z_far;

			float shadow = 0.0;
			
			float view_distance = length(frag_pos - camera_position.xyz);
			int samples = 20;
			float sampling_radius = 0.003;
			for (int i = 0; i < samples; i++)
			{
				float closest_depth = texture(shadow_map, fragToLight + sampling_offsets[i] * sampling_radius).r;
				shadow += current_depth - bias < closest_depth ? 1.0 : 0.0;
			}

			shadow /= float(samples);

			return clamp(shadow, 0.0, 1.0);
		}
	#elif LIGHT_TYPE == 1
		float get_shadow_dir(vec3 frag_pos, float bias)
		{
			// Select layer based on depth
			float depth = ((view * vec4(frag_pos, 1.0)).z);
			int layer = 0;
			for (int i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
			{
				if (depth < ubo.cascade_splits[i])
					layer = i + 1;
			}

			vec4 frag_pos_light_space = ubo.light_matrix[layer] * vec4(frag_pos, 1.0);
			vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;

			float current_depth = proj_coords.z;
			//if (layer == 1)
			//	return 0;
			if (current_depth > 1.0)
				return 1.0;

			const float biasModifier = 0.5f;
			if (layer == SHADOW_MAP_CASCADE_COUNT)
			{
				//bias *= 1 / (200.0f * biasModifier);
			} else
			{
				//bias *= 1 / (ubo.cascade_splits[layer] * biasModifier);
			}
			//bias = 0.005;
			float shadow = 0.0;
			
			// PCF
			float tex_size = textureSize(shadow_map, 0).x;
			int count = 0;
			float scale = 1.0f / (1.0 + layer);
			for (int x = -1; x <= 1; x++)
			{
				for (int y = -1; y <= 1; y++)
				{
					float closest_depth = texture(shadow_map, vec3((proj_coords.xy * 0.5 + 0.5) + vec2(x, y) / tex_size * scale, layer)).r;
					shadow += current_depth - bias < closest_depth ? 1.0 : 0.0;
					count++;
				}
			}
			shadow /= float(count);
			//float closest_depth = texture(shadow_map, vec3(proj_coords.xy * 0.5 + 0.5, layer)).r;
			//shadow += current_depth - bias < closest_depth ? 1.0 : 0.0;
			return clamp(shadow, 0.0, 1.0);
		}
	#endif
#endif

void main()
{
	vec2 inUV = inPos.xy / inPos.w * 0.5 + 0.5;
	float depth = texture(textures[depthTexId], inUV).r;

	vec4 shading = texture(textures[shadingTexId], inUV).rgba;
	float metalness = shading.r;
	float roughness = clamp(shading.g, 0.045f, 1.0f);
	roughness *= roughness;
	float specular = shading.b;

	vec3 albedo = texture(textures[albedoTexId], inUV).rgb;
	// Metals only reflect light, while dielectrics has diffuse light
	vec3 diffuse_color = albedo * (1.0f - metalness);
	float reflectance = compute_reflectance(specular);

	// As I said before, albedo as reflection for metallic, and reflectance based for non-metals
	vec3 F0 = (albedo * metalness) + (reflectance * (1.0f - metalness));
	float F90 = 1.0f;


	vec3 view_pos = getVSPosition(inUV, depth);
	vec4 world_pos = inverse(view) * vec4(view_pos, 1.0);
	world_pos.xyz /= world_pos.w;
	vec3 P = world_pos.xyz;
	vec3 N = normalize(texture(textures[normalTexId], inUV).rgb);
	vec3 V = normalize(camera_position.xyz - P);
	vec3 L;
	float light_attenuation = 1.0;
	float shadow = 1.0f;

	#if RAY_TRACED_SHADOWS == 1
		L = normalize(PushConstants.light_pos.xyz);
		#if USE_SHADOWS == 1
			shadow = texture(shadow_map, inUV).r;
		#endif
	#else
		#if LIGHT_TYPE == 0
			// Punctual light
			L = normalize(PushConstants.light_pos.xyz - P);
			light_attenuation = get_attenuation(P);
			#if USE_SHADOWS == 1
				float bias = max(0.005, 0.05 * (1.0 - dot(N, L)));
				shadow = get_shadow_point(P, bias);
			#endif
		#else
			// Directional light
			L = normalize(PushConstants.light_pos.xyz);
			#if USE_SHADOWS == 1
				float bias = max(0.002, 0.002 * (1.0 - dot(N, L)));
				shadow = get_shadow_dir(P, bias);
			#endif
		#endif
	#endif
	vec3 H = normalize(V + L);

	float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
	float NdotV = clamp(abs(dot(N, V)), 1e-4, 1.0f);
	float NdotH = clamp(dot(N, H), 0.0f, 1.0f);
	float LdotH = clamp(dot(L, H), 0.0f, 1.0f);

	// Diffuse BRDF
	float F_diffuse = Fr_DisneyDiffuse(NdotV, NdotL, LdotH, roughness);
	vec3 diffuse = diffuse_color * F_diffuse;

	// Specular BRDF
	vec3 F = F_Schlick(F0, F90, LdotH);
	float D = D_GGX(NdotH, roughness * roughness);
	float Viz = V_SmithGGXCorrelated(NdotV, NdotL, roughness); 
	vec3 F_specular = D * F * Viz;

	outDiffuse = shadow * NdotL * (vec3(1.0f) - F) * diffuse * light_attenuation * PushConstants.light_intensity * PushConstants.light_color.rgb;
	outSpecular = shadow * NdotL * F_specular * light_attenuation * PushConstants.light_intensity * PushConstants.light_color.rgb;
}