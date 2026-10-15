#pragma once
#include "Scene/Entity.h"
#include "Renderers/DebugRenderer.h"
#include "EditorContext.h"
#include "imgui.h"

class HierarchyPanel
{
public:
	void renderImGui(EditorContext &context);

private:
	entt::entity start_entity = entt::null;
	ImGuiTextFilter filter;

	entt::entity renaming_entity = entt::null;
	char rename_buffer[256]{};
};
