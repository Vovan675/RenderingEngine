#pragma once
#include "RHI/Buffer.h"
#include "RHI/Device.h"
#include "Log.h"
#include "Mesh.h"
#include "Scene/Scene.h"
#include "RHI/Pipeline.h"
#include "RHI/Shader.h"
#include "RHI/Swapchain.h"
#include "RHI/Texture.h"
#include "RHI/VkWrapper.h"
#include "renderers/EntityRenderer.h"
#include "renderers/LutRenderer.h"
#include "renderers/IrradianceRenderer.h"
#include "renderers/PrefilterRenderer.h"
#include "renderers/SkyRenderer.h"
#include "renderers/MeshRenderer.h"
#include "imgui/ImGuiWrapper.h"
#include "renderers/DefferedLightingRenderer.h"
#include "renderers/DefferedCompositeRenderer.h"
#include "renderers/PostProcessingRenderer.h"
#include "renderers/DebugRenderer.h"
#include "renderers/SSAORenderer.h"
#include "Editor/ViewportPanel.h"
#include "Editor/AssetBrowserPanel.h"
#include "Editor/ParametersPanel.h"
#include "Editor/HierarchyPanel.h"
#include "Editor/DebugPanel.h"
#include "Application.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "RHI/RayTracing/RayTracingScene.h"
#include "ConsoleVariables.h"

class EditorApplication : public Application
{
public:
	EditorApplication();
protected:
	void update(float delta_time) override;
	void updateBuffers(float delta_time) override;
	void recordCommands(CommandBuffer &command_buffer) override;
	void cleanupResources() override;
private:
	void render_GBuffer(CommandBuffer &command_buffer);
	void render_ray_tracing(CommandBuffer &command_buffer);
	void render_lighting(CommandBuffer &command_buffer);
	void render_ssao(CommandBuffer &command_buffer);
	void render_deffered_composite(CommandBuffer &command_buffer);
	void render_shadows(CommandBuffer &command_buffer);

	void update_cascades(LightComponent &light, glm::vec3 light_dir);
private:
	std::shared_ptr<RayTracingScene> ray_tracing_scene;

	EntityRenderer entity_renderer;

	EditorContext context;

	ViewportPanel viewport_panel;
	AssetBrowserPanel asset_browser_panel;
	ParametersPanel parameters_panel;
	HierarchyPanel hierarchy_panel;
	DebugPanel debug_panel;

	std::shared_ptr<LutRenderer> lut_renderer;
	std::shared_ptr<IrradianceRenderer> irradiance_renderer;
	std::shared_ptr<PrefilterRenderer> prefilter_renderer;

	std::shared_ptr<SkyRenderer> sky_renderer;
	std::shared_ptr<DefferedLightingRenderer> defferred_lighting_renderer;
	std::shared_ptr<DefferedCompositeRenderer> deffered_composite_renderer;
	std::shared_ptr<PostProcessingRenderer> post_renderer;
	std::shared_ptr<DebugRenderer> debug_renderer;
	std::shared_ptr<SSAORenderer> ssao_renderer;

	std::vector<std::shared_ptr<RendererBase>> renderers;

	std::shared_ptr<Shader> gbuffer_vertex_shader;
	std::shared_ptr<Shader> gbuffer_fragment_shader;

	std::shared_ptr<Shader> shadows_vertex_shader;
	std::shared_ptr<Shader> shadows_fragment_shader_point;
	std::shared_ptr<Shader> shadows_fragment_shader_directional;

	std::shared_ptr<Texture> storage_image;
	std::shared_ptr<Shader> raygen_shader;
	std::shared_ptr<Shader> miss_shader;
	std::shared_ptr<Shader> closest_hit_shader;
};