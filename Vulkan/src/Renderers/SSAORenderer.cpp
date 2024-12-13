#include "pch.h"
#include "SSAORenderer.h"
#include "imgui.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Math.h"
#include <random>

SSAORenderer::SSAORenderer() : RendererBase()
{
	TextureDescription desc {};
	desc.width = 4;
	desc.height = 4;
	desc.imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	desc.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
	desc.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;
	desc.filtering = VK_FILTER_NEAREST;
	desc.anisotropy = false;
	
	ssao_noise = Texture::create(desc);

	std::default_random_engine generator(0);
	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	
	const int kernel_size = 64;
	for (unsigned int i = 0; i < kernel_size; i++)
	{
		glm::vec3 sample(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator)); // Because hemisphere in tangent space
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);

		// Make closer to origin
		float scale = (float)i / (float)kernel_size;
		scale = Engine::Math::lerp(0.1f, 1.0f, scale * scale);

		ssao_kernel.push_back(glm::vec4(sample * scale, 0.0f));
	}
	memcpy(ubo_raw_pass.kernel, ssao_kernel.data(), sizeof(ubo_raw_pass.kernel));

	std::vector<glm::vec4> ssao_noise_data;
	for (unsigned int i = 0; i < 16; i++)
	{
		glm::vec4 noise(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0,
			0);
		ssao_noise_data.push_back(noise);
	}

	ssao_noise->fill(ssao_noise_data.data());

	fragment_shader_raw = Shader::create("shaders/ssao.frag", Shader::FRAGMENT_SHADER);
	fragment_shader_blur = Shader::create("shaders/ssao_blur.frag", Shader::FRAGMENT_SHADER);
}

void SSAORenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	ubo_raw_pass.normal_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_NORMAL);
	ubo_raw_pass.depth_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_DEPTH_STENCIL);
	ubo_blur_pass.raw_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO_RAW);

	// Raw Pass
	Renderer::getRenderTarget(RENDER_TARGET_SSAO_RAW)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

	VkWrapper::cmdBeginRendering(command_buffer, {Renderer::getRenderTarget(RENDER_TARGET_SSAO_RAW)}, nullptr);

	auto &p = VkWrapper::global_pipeline;
	p->bindScreenQuadPipeline(command_buffer, fragment_shader_raw);

	// Uniforms
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_raw_pass, sizeof(UBO_RAW));
	Renderer::setShadersTexture(p->getCurrentShaders(), 1, ssao_noise);
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
	VkWrapper::cmdEndRendering(command_buffer);


	// Blur Pass
	Renderer::getRenderTarget(RENDER_TARGET_SSAO_RAW)->transitLayout(command_buffer, TEXTURE_LAYOUT_SHADER_READ);
	Renderer::getRenderTarget(RENDER_TARGET_SSAO)->transitLayout(command_buffer, TEXTURE_LAYOUT_ATTACHMENT);

	VkWrapper::cmdBeginRendering(command_buffer, {Renderer::getRenderTarget(RENDER_TARGET_SSAO)}, nullptr);

	p = VkWrapper::global_pipeline;
	p->bindScreenQuadPipeline(command_buffer, fragment_shader_blur);

	// Bindless
	vkCmdBindDescriptorSets(command_buffer.get_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, p->getPipelineLayout(), 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	// Uniforms
	ubo_blur_pass.raw_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO_RAW);
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo_blur_pass, sizeof(UBO_BLUR));
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
	VkWrapper::cmdEndRendering(command_buffer);
}

void SSAORenderer::renderImgui()
{
	ImGui::SliderInt("SSAO Samples", &ubo_raw_pass.samples, 2, 64);
	ImGui::SliderFloat("SSAO Sample Radius", &ubo_raw_pass.sample_radius, 0.01f, 1.0f);
}
