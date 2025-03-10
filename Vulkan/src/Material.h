#pragma once
#include "BindlessResources.h"
#include "RHI/RHITexture.h"
#include "YamlExtensions.h"
#include "Assets/AssetManager.h"
#include "Utils/Stream.h"

class Material : public RefCounted
{
public:
		int albedo_tex_id = -1;
		glm::vec4 albedo = glm::vec4(1, 1, 1, 1);

		int metalness_tex_id = -1;
		float metalness = 0.0f;

		int roughness_tex_id = -1;
		float roughness = 1.0f;

		int specular_tex_id = -1;
		float specular = 0.5f;

		int normal_tex_id = -1;

		struct PushConstant 
		{
			glm::mat4 model;

			glm::vec4 albedo = glm::vec4(0, 0, 0, 1);
			int albedo_tex_id = -1;
			int metalness_tex_id = -1;
			int roughness_tex_id = -1;
			int specular_tex_id = -1;
			// metalness, roughness, specular
			glm::vec4 shading = glm::vec4(0, 0, 0.5, 1);
			int normal_tex_id = -1;
		};

		PushConstant getPushConstant(glm::mat4 model) const
		{
			PushConstant push_constant;
			push_constant.model = model;
			push_constant.albedo = albedo;
			push_constant.albedo_tex_id = albedo_tex_id;
			push_constant.metalness_tex_id = metalness_tex_id;
			push_constant.roughness_tex_id = roughness_tex_id;
			push_constant.specular_tex_id = specular_tex_id;
			push_constant.shading = glm::vec4(metalness, roughness, specular, 1.0f);
			push_constant.normal_tex_id = normal_tex_id;
			return push_constant;
		}

		void serialize(Stream &stream)
		{
			stream.write(albedo);
			stream.write(metalness);
			stream.write(roughness);
			stream.write(specular);

			const auto write_tex = [&stream](int tex_id)
			{
				if (tex_id == -1)
					stream.write(std::string());
				else
					stream.write(gDynamicRHI->getBindlessResources()->getTexture(tex_id)->getPath());
			};

			write_tex(albedo_tex_id);
			write_tex(metalness_tex_id);
			write_tex(roughness_tex_id);
			write_tex(specular_tex_id);
			write_tex(normal_tex_id);
		}

		void deserialize(Stream &stream)
		{
			stream.read(albedo);
			stream.read(metalness);
			stream.read(roughness);
			stream.read(specular);

			TextureDescription non_srgb_description = {};
			non_srgb_description.format = FORMAT_R8G8B8A8_UNORM;
			non_srgb_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

			const auto read_tex = [&stream, &non_srgb_description](int &tex_id, bool non_srgb = false)
			{
				std::string tex_path;
				stream.read(tex_path);
				if (tex_path.empty())
				{
					tex_id = -1;
					return;
				}

				auto tex = non_srgb ? AssetManager::getTextureAsset(tex_path, non_srgb_description) : AssetManager::getTextureAsset(tex_path);
				tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			};

			read_tex(albedo_tex_id);
			read_tex(metalness_tex_id);
			read_tex(roughness_tex_id);
			read_tex(specular_tex_id);
			read_tex(normal_tex_id, true);
		}
};

namespace YAML
{
	static YAML::Emitter &operator <<(YAML::Emitter &out, const Ref<Material> &mat)
	{
		out << YAML::BeginMap;
		out << YAML::Key << "Albedo" << YAML::Value << mat->albedo;
		out << YAML::Key << "Metalness" << YAML::Value << mat->metalness;
		out << YAML::Key << "Roughness" << YAML::Value << mat->roughness;
		out << YAML::Key << "Specular" << YAML::Value << mat->specular;
		out << YAML::Key << "Normal" << YAML::Value << mat->specular;
		
		std::string no_texture = "no";
		if (mat->albedo_tex_id != -1)
			out << YAML::Key << "AlbedoTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->albedo_tex_id)->getPath();

		if (mat->metalness_tex_id != -1)
			out << YAML::Key << "MetalnessTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->metalness_tex_id)->getPath();

		if (mat->roughness_tex_id != -1)
			out << YAML::Key << "RoughnessTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->roughness_tex_id)->getPath();

		if (mat->specular_tex_id != -1)
			out << YAML::Key << "SpecularTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->specular_tex_id)->getPath();

		if (mat->normal_tex_id != -1)
			out << YAML::Key << "NormalTexture" << YAML::Value << gDynamicRHI->getBindlessResources()->getTexture(mat->normal_tex_id)->getPath();
		out << YAML::EndMap;
		return out;
	}

	template<>
	struct convert<Ref<Material>>
	{
		static bool decode(const Node &node, Ref<Material> &mat)
		{
			if (!node.IsMap())
				return false;

			mat = new Material();
			mat->albedo = node["Albedo"].as<glm::vec4>();
			mat->metalness = node["Metalness"].as<float>();
			mat->roughness = node["Roughness"].as<float>();
			mat->specular = node["Specular"].as<float>();

			if (node["AlbedoTexture"])
			{
				auto tex = AssetManager::getTextureAsset(node["AlbedoTexture"].as<std::string>());
				mat->albedo_tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["MetalnessTexture"])
			{
				auto tex = AssetManager::getTextureAsset(node["MetalnessTexture"].as<std::string>());
				mat->metalness_tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["RoughnessTexture"])
			{
				auto tex = AssetManager::getTextureAsset(node["RoughnessTexture"].as<std::string>());
				mat->roughness_tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["SpecularTexture"])
			{
				auto tex = AssetManager::getTextureAsset(node["SpecularTexture"].as<std::string>());
				mat->specular_tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			if (node["NormalTexture"])
			{
				TextureDescription tex_description{};
				tex_description.format = FORMAT_R8G8B8A8_UNORM;
				tex_description.usage_flags = TEXTURE_USAGE_TRANSFER_SRC;

				auto tex = AssetManager::getTextureAsset(node["NormalTexture"].as<std::string>(), tex_description);
				mat->normal_tex_id = gDynamicRHI->getBindlessResources()->addTexture(tex);
			}

			return true;
		}
	};
}
