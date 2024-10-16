#include "pch.h"
#include "imgui.h"
#include "PostProcessingRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

PostProcessingRenderer::PostProcessingRenderer()
{
	vertex_shader = Shader::create("shaders/quad.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/post_processing.frag", Shader::FRAGMENT_SHADER);
}

PostProcessingRenderer::~PostProcessingRenderer()
{
}

void PostProcessingRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader);
	p->setFragmentShader(fragment_shader);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseVertices(false);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(UBO));
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void PostProcessingRenderer::renderImgui()
{
	bool use_vignette_bool = ubo.use_vignette > 0.5f;
	if (ImGui::Checkbox("Vignette", &use_vignette_bool))
	{
		ubo.use_vignette = use_vignette_bool ? 1.0f : 0.0f;
	}

	if (ubo.use_vignette)
	{
		ImGui::SliderFloat("Vignette Radius", &ubo.vignette_radius, 0.1f, 1.0f);
		ImGui::SliderFloat("Vignette Smoothness", &ubo.vignette_smoothness, 0.1f, 1.0f);
	}
}
