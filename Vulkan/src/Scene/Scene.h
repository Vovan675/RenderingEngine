#pragma once
#include "entt/entt.hpp"
#include "Components.h"

class Entity;

class Scene
{
public:
	Scene() = default;

	Entity createEntity(std::string name);
	Entity createEntity(std::string name, entt::entity id);

	void destroyEntity(entt::entity id);

	template<typename ...T>
	auto getEntitiesWith()
	{
		return registry.view<T...>();
	}

	void saveFile(const std::string &filename);
	void loadFile(const std::string &filename);

	static std::shared_ptr<Scene> getCurrentScene() { return current_scene; }
	static std::shared_ptr<Scene> loadScene(const std::string &filename);
	static void closeScene();
private:
	friend class Entity;
	entt::registry registry;

	static std::shared_ptr<Scene> current_scene;
};
