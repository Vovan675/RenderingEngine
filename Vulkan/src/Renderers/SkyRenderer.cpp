#include "pch.h"
#include "SkyRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"
#include "Model.h"
#include "Utils/Math.h"
#include "Variables.h"
#include <imgui.h>

SkyRenderer::SkyRenderer(): RendererBase()
{
	setMode(SKY_MODE_CUBEMAP);

	auto model = AssetManager::getModelAsset("assets/cube.fbx");
	mesh = model->getRootNode()->children[0]->meshes[0];

	vertex_shader = Shader::create("shaders/cube.vert", Shader::VERTEX_SHADER);
	fragment_shader = Shader::create("shaders/cube.frag", Shader::FRAGMENT_SHADER);
}

void SkyRenderer::addProceduralPasses(FrameGraph &fg)
{
	auto &sky_data = fg.getBlackboard().add<SkyData>();
	FrameGraphResource sky_resource = importTexture(fg, cube_texture);
	sky_data.sky = sky_resource;

	if (mode != SKY_MODE_PROCEDURAL)
		return;

	if (!isDirty())
		return;
	prev_uniform = procedural_uniforms;

	sky_data = fg.addCallbackPass<SkyData>("Sky Procedural Pass",
	[&](RenderPassBuilder &builder, SkyData &data)
	{
		data.sky = builder.write(sky_resource);
	},
	[=](const SkyData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		auto &sky_texture = resources.getResource<FrameGraphTexture>(data.sky);

		for (int face = 0; face < 6; face++)
		{
			VkWrapper::cmdBeginRendering(command_buffer, {sky_texture.texture}, nullptr, face);
			auto &p = VkWrapper::global_pipeline;
			p->reset();

			p->setVertexShader(Shader::create("shaders/ibl/cubemap_filter.vert", Shader::VERTEX_SHADER));
			p->setFragmentShader(Shader::create("shaders/procedural_sky.frag", Shader::FRAGMENT_SHADER));

			p->setRenderTargets(VkWrapper::current_render_targets);
			p->setCullMode(VK_CULL_MODE_BACK_BIT);
			p->setDepthTest(false);

			p->flush();
			p->bind(command_buffer);

			// Uniforms
			Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &procedural_uniforms, sizeof(Uniforms));
			Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

			struct PushConstant
			{
				glm::mat4 mvp = glm::mat4(1.0f);
			} constants;
			constants.mvp = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 512.0f) * Math::getCubeFaceTransform(face);
			vkCmdPushConstants(command_buffer.get_buffer(), p->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstant), &constants);

			// Render mesh
			VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
			VkDeviceSize offsets[] = {0};
			vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);

			p->unbind(command_buffer);

			VkWrapper::cmdEndRendering(command_buffer);
		}
	});

}

void SkyRenderer::addCompositePasses(FrameGraph &fg)
{
	auto &sky_data = fg.getBlackboard().get<SkyData>();
	auto &default_data = fg.getBlackboard().get<DefaultResourcesData>();

	is_force_dirty = false;
	default_data = fg.addCallbackPass<DefaultResourcesData>("Sky Pass",
	[&](RenderPassBuilder &builder, DefaultResourcesData &data)
	{
		// Setup
		data = default_data;
		data.final_no_post = builder.write(default_data.final_no_post);
		builder.read(sky_data.sky);
	},
	[=](const DefaultResourcesData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &composite = resources.getResource<FrameGraphTexture>(data.final_no_post);
		auto &sky = resources.getResource<FrameGraphTexture>(sky_data.sky);

		VkWrapper::cmdBeginRendering(command_buffer, {composite.texture}, nullptr);

		auto &p = VkWrapper::global_pipeline;

		p->reset();

		p->setVertexShader(Shader::create("shaders/cube.vert", Shader::VERTEX_SHADER));
		p->setFragmentShader(Shader::create("shaders/cube.frag", Shader::FRAGMENT_SHADER));
		p->setUseBlending(false);

		p->setRenderTargets(VkWrapper::current_render_targets);
		p->setCullMode(VK_CULL_MODE_FRONT_BIT);

		p->flush();
		p->bind(command_buffer);

		// Uniforms
		Renderer::setShadersTexture(p->getCurrentShaders(), 1, sky.texture);
		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		// Render mesh
		VkBuffer vertexBuffers[] = {mesh->vertexBuffer->bufferHandle};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(command_buffer.get_buffer(), mesh->indexBuffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(command_buffer.get_buffer(), mesh->indices.size(), 1, 0, 0, 0);
		p->unbind(command_buffer);

		VkWrapper::cmdEndRendering(command_buffer);
	});
}

void SkyRenderer::renderImgui()
{
	bool is_procedural = mode == SKY_MODE_PROCEDURAL;
	if (ImGui::Checkbox("Procedural Sky Enabled", &is_procedural))
		setMode(is_procedural ? SKY_MODE_PROCEDURAL : SKY_MODE_CUBEMAP);

	if (is_procedural)
	{
		ConVarSystem::drawConVarImGui(render_automatic_sun_position.getDescription());
		if (!render_automatic_sun_position)
			ImGui::SliderFloat3("Sun Dir", procedural_uniforms.sun_direction.data.data, -1.0f, 1.0f);
	}
}

void SkyRenderer::setMode(SKY_MODE mode)
{
	if (this->mode == mode)
		return;

	this->mode = mode;

	TextureDescription tex_description{};
	tex_description.width = 1024;
	tex_description.height = 1024;
	tex_description.image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
	tex_description.usage_flags = TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_SRC | TEXTURE_USAGE_TRANSFER_DST;
	tex_description.is_cube = true;

	cube_texture = Texture::create(tex_description);

	if (mode == SKY_MODE_CUBEMAP)
		cube_texture->loadCubemap("assets/cubemap/posx.jpg", "assets/cubemap/negx.jpg",
								  "assets/cubemap/posy.jpg", "assets/cubemap/negy.jpg",
								  "assets/cubemap/posz.jpg", "assets/cubemap/negz.jpg");
	else
		cube_texture->fill();
	cube_texture->setDebugName("Sky Texture");
	is_force_dirty = true;
}

bool SkyRenderer::isDirty()
{
	if (mode != SKY_MODE_PROCEDURAL)
		return false;
	bool dirty = is_force_dirty;
	if (prev_uniform.sun_direction != procedural_uniforms.sun_direction)
		dirty = true;
	if (render_first_frame)
		dirty = true;
	return dirty;
}
