﻿#include "pch.h"
#include "EditorApplication.h"
#include "RHI/VkWrapper.h"
#include "Scene/Entity.h"
#include "Rendering/Renderer.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "Filesystem.h"
#include "Editor/EditorDefaultScene.h"

#include "Variables.h"

#include "Editor/GuiUtils.h"

#include "FrameGraph/FrameGraph.h"
#include "FrameGraph/GraphViz.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphRHIResources.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Physics/PhysXWrapper.h"

using namespace physx;

static bool is_play = false;

EditorApplication::EditorApplication()
{
	context.editor_camera = std::make_shared<Camera>();
	Renderer::setCamera(context.editor_camera);

	Scene::setCurrentScene(std::make_shared<Scene>());
	EditorDefaultScene::createScene();

	ImGuiWrapper::init(window);
	
	shadow_renderer.context = &context;
	shadow_renderer.debug_renderer = &debug_renderer;
	shadow_renderer.entity_renderer = &entity_renderer;

	gbuffer_pass.entity_renderer = &entity_renderer;

	if (engine_ray_tracing)
	{
		ray_tracing_scene = std::make_shared<RayTracingScene>(nullptr);
		shadow_renderer.ray_tracing_scene = ray_tracing_scene;
	}
}

void EditorApplication::update(float delta_time)
{
	ImGuiWrapper::begin();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushFont(GuiUtils::roboto_regular_small);
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("Save"))
			{
				std::string path = Filesystem::saveFileDialog();
				if (!path.empty())
					Scene::getCurrentScene()->saveFile(path);
			}

			if (ImGui::MenuItem("Load"))
			{
				std::string path = Filesystem::openFileDialog();
				if (!path.empty())
				{
					Scene::setCurrentScene(std::make_shared<Scene>());
					Scene::getCurrentScene()->loadFile(path);
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	float menu_bar_height = ImGui::GetFrameHeight();
	ImGui::PopFont();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	//ImGui::ShowStyleEditor();
	//ImGui::ShowDemoWindow();

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos({viewport->Pos.x, viewport->Pos.y + menu_bar_height});
	ImGui::SetNextWindowSize({viewport->Size.x, viewport->Size.y - menu_bar_height});
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin("DockSpace", nullptr, window_flags);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();

	ImGui::DockSpace(ImGui::GetID("EngineDockSpace"));

	// Viewport
	static bool prev_is_viewport_focused = false;
	bool is_viewport_focused = viewport_panel.renderImGui(context, delta_time);

	// TODO: move to DebugPanel
	// Debug Panel
	debug_panel.sky_renderer = &sky_renderer;
	debug_panel.defferred_lighting_renderer = &defferred_lighting_renderer;
	debug_panel.post_renderer = &post_renderer;
	debug_panel.debug_renderer = &debug_renderer;
	debug_panel.ssao_renderer = &ssao_renderer;
	debug_panel.renderImGui(context);
	bool play_clicked = ImGui::Checkbox("Play", &is_play);

	ImGui::End();

	// Hierarchy
	hierarchy_panel.renderImGui(context);
	ImGui::End();

	// Parameters
	parameters_panel.renderImGui(context.selected_entity, debug_renderer);

	// Asset browser
	asset_browser_panel.renderImGui();

	if (input.isKeyDown(GLFW_KEY_ESCAPE))
		context.selected_entity = Entity();

	viewport_panel.update();

	if (!ImGuizmo::IsUsing() && is_viewport_focused)
	{
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		bool mouse_pressed = prev_is_viewport_focused != is_viewport_focused ? 0 : glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
		context.editor_camera->update(delta_time, glm::vec2(mouse_x, mouse_y), mouse_pressed);
	}
	context.editor_camera->updateMatrices();

	prev_is_viewport_focused = is_viewport_focused;

	static std::shared_ptr<Scene> saved_scene = nullptr;
	if (play_clicked)
	{
		context.selected_entity = Entity();
		if (is_play)
		{
			// Copy scene
			saved_scene = Scene::getCurrentScene();
			Scene::setCurrentScene(saved_scene->copy());
			Scene::getCurrentScene()->physics_scene->reinit();
		} else
		{
			// Revert scene
			Scene::setCurrentScene(saved_scene);
			saved_scene = nullptr;
		}
	}


	Scene::getCurrentScene()->physics_scene->draw_debug(&debug_renderer);
	if (is_play)
	{
		Scene::getCurrentScene()->updateRuntime();
	}
}

void EditorApplication::updateBuffers(float delta_time)
{
	Renderer::updateDefaultUniforms(delta_time);

	ssao_renderer.ubo_raw_pass.near_plane = context.editor_camera->getNear();
	ssao_renderer.ubo_raw_pass.far_plane = context.editor_camera->getFar();
}

std::vector<RenderBatch> render_batches;

void EditorApplication::recordCommands(CommandBuffer &command_buffer)
{
	render_batches.clear();

	BoundFrustum bound_frustum(context.editor_camera->getProj(), context.editor_camera->getView());
	auto entities_id = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
	for (entt::entity entity_id : entities_id)
	{
		Entity entity(entity_id);
		MeshRendererComponent &mesh_renderer = entity.getComponent<MeshRendererComponent>();

		for (int i = 0; i < mesh_renderer.meshes.size(); i++)
		{
			const std::shared_ptr<Engine::Mesh> mesh = mesh_renderer.meshes[i].getMesh();
			const auto material = mesh_renderer.materials.size() > i ? mesh_renderer.materials[i] : std::make_shared<Material>();

			RenderBatch &batch = render_batches.emplace_back();
			batch.mesh = mesh;
			batch.material = material;
			batch.world_transform = entity.getWorldTransformMatrix();
			auto bbox = mesh->bound_box * batch.world_transform;
			batch.camera_visible = bbox.isInside(bound_frustum);
		}
	}


	FrameGraph frameGraph;

	DefaultResourcesData &default_data = frameGraph.getBlackboard().add<DefaultResourcesData>();
	default_data.final_no_post = importTexture(frameGraph, Renderer::getRenderTarget(RENDER_TARGET_FINAL_NO_POST));
	default_data.final = importTexture(frameGraph, Renderer::getRenderTarget(RENDER_TARGET_FINAL));
	default_data.backbuffer = importTexture(frameGraph, VkWrapper::swapchain->swapchain_textures[Renderer::getCurrentImageIndex()]);

	if (engine_ray_tracing)
		ray_tracing_scene->update();

	bool is_sky_dirty = sky_renderer.isDirty();
	sky_renderer.addProceduralPasses(frameGraph);

	LutData &lut_data = frameGraph.getBlackboard().add<LutData>();
	lut_data.brdf_lut = importTexture(frameGraph, lut_renderer.brdf_lut_texture);

	if (render_first_frame)
	{
		// Render BRDF Lut
		lut_renderer.addPasses(frameGraph);
	}

	IBLData &ibl_data = frameGraph.getBlackboard().add<IBLData>();
	ibl_data.irradiance = importTexture(frameGraph, irradiance_renderer.irradiance_texture);
	ibl_data.prefilter = importTexture(frameGraph, prefilter_renderer.prefilter_texture);

	if (render_first_frame || is_sky_dirty)
	{
		// Render IBL irradiance
		irradiance_renderer.addPass(frameGraph);

		// Render IBL prefilter
		prefilter_renderer.addPass(frameGraph);
	}
	render_first_frame = false;

	gbuffer_pass.AddPass(frameGraph, render_batches);

	// Shadows
	{
		if (render_shadows)
			shadow_renderer.addShadowMapPasses(frameGraph, render_batches);

		if (render_ray_traced_shadows)
			shadow_renderer.addRayTracedShadowPasses(frameGraph);
	}

	defferred_lighting_renderer.renderLights(frameGraph);

	ssao_renderer.addPasses(frameGraph);
	if (render_ssr)
		ssr_renderer.addPasses(frameGraph);

	auto &composite_data = frameGraph.getBlackboard().add<CompositeData>();
	// Draw Sky on background
	sky_renderer.addCompositePasses(frameGraph);
	// Draw composite (discard sky pixels by depth)
	deffered_composite_renderer.addPasses(frameGraph);

	// Render post process
	post_renderer.addPasses(frameGraph);

	// Render debug textures
	if (render_debug_rendering)
	{
		debug_renderer.addPasses(frameGraph);
	}

	debug_renderer.renderLines(frameGraph);

	// Render ImGui to backbuffer
	default_data = frameGraph.addCallbackPass<DefaultResourcesData>("ImGui Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		data = default_data;
		data.final = builder.read(default_data.final);
		data.backbuffer = builder.write(default_data.backbuffer, RenderPassNode::RESOURCE_ACCESS_IGNORE_FLAG);
		builder.setSideEffect(true);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		auto &final = resources.getResource<FrameGraphTexture>(data.final);
		auto &backbuffer = resources.getResource<FrameGraphTexture>(data.backbuffer);

		VkWrapper::cmdBeginRendering(command_buffer, {VkWrapper::swapchain->swapchain_textures[Renderer::getCurrentImageIndex()]}, nullptr);
		ImGuiWrapper::render(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});

	frameGraph.compile();
	frameGraph.execute(command_buffer);

	if (input.isKeyDown(GLFW_KEY_T))
	{
		GraphViz viz;
		viz.show("graph.dot", frameGraph);
	}
}

void EditorApplication::cleanupResources()
{
	ImGuiWrapper::shutdown();
	shadow_renderer.ray_tracing_scene = nullptr;
	ray_tracing_scene = nullptr;
}
