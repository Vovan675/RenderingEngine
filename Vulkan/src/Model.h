#pragma once
#include "Mesh.h"
#include <vector>
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/material.h"
#include "assimp/pbrmaterial.h"
#include "Utils/FileStream.h"
#include "Scene/Entity.h"
#include "Material.h"

struct MeshNode
{
	~MeshNode()
	{
		for (size_t i = 0; i < children.size(); i++)
			delete children[i];
	}

	std::string name = "";
	std::vector<Engine::Mesh *> meshes;
	std::vector<Ref<Material>> materials;
	MeshNode *parent = nullptr;
	std::vector<MeshNode *> children;
	
	glm::mat4 local_model_matrix = glm::mat4(1);
	glm::mat4 global_model_matrix = glm::mat4(1);

	void updateTransform()
	{
		if (parent)
		{
			global_model_matrix = parent->global_model_matrix * local_model_matrix;
		} else
		{
			global_model_matrix = local_model_matrix;
		}

		for (auto &child : children)
		{
			child->updateTransform();
		}
	}
};

// Model is an asset that contains one full model (for instance, fbx file). It can be used to get information about model and for creating entities from it
class Model : public Asset
{
public:
	Model() = default;
	~Model();
	void cleanup();

	ASSET_TYPE getAssetType() const override { return ASSET_TYPE_MODEL; }

	void load(const char *path);
	void process_node(MeshNode *parent_node, aiNode *node, const aiScene *scene);

	static Entity createEntity(Model *model);

	Engine::Mesh *getMesh(size_t id)
	{
		if (meshes_id.find(id) == meshes_id.end())
			return nullptr;
		return meshes_id[id];
	}

	MeshNode *getRootNode() const { return root_node; }
	std::vector<MeshNode *> &getLinearNodes() { return linear_nodes; }

	std::string getPath() const { return path; }

	void saveFile(const std::string &filename);
	void loadFile(const std::string &filename);

private:
	void save_mesh_node(FileStream &stream, MeshNode *node);
	void load_mesh_node(FileStream &stream, MeshNode *node);
	static Entity create_entity_node(Model *model, MeshNode *node, Scene *scene);

	MeshNode *root_node = nullptr;
	std::vector<MeshNode *> linear_nodes = {};
	std::string path;

	std::unordered_map<size_t, Ref<Engine::Mesh>> meshes_id;
};

