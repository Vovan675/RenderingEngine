#include "pch.h"
#include "GBufferPass.h"
#include "Rendering/Renderer.h"
#include "FrameGraph/FrameGraphData.h"
#include "FrameGraph/FrameGraphUtils.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"

GBufferPass::GBufferPass()
{
	gbuffer_vertex_shader = Shader::create("shaders/opaque.vert", Shader::VERTEX_SHADER);
	gbuffer_fragment_shader = Shader::create("shaders/opaque.frag", Shader::FRAGMENT_SHADER);
}

void GBufferPass::AddPass(FrameGraph &fg)
{
	auto &command_buffer = Renderer::getCurrentCommandBuffer(); // TODO: pass command buffer to render part of callback pass

	auto &gbuffer_data = fg.getBlackboard().add<GBufferData>();

	gbuffer_data = fg.addCallbackPass<GBufferData>("GBuffer Pass",
	[&](RenderPassBuilder &builder, GBufferData &data)
	{
		// Setup
		FrameGraphTexture::Description desc;
		desc.width = Renderer::getViewportSize().x;
		desc.height = Renderer::getViewportSize().y;

		auto create_texture = [&desc, &builder](VkFormat format, uint32_t usage_flags, const char *name = nullptr, VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT)
		{
			desc.debug_name = name;
			desc.image_format = format;
			desc.usage_flags = usage_flags;
			desc.sampler_address_mode = sampler_address_mode;

			auto resource = builder.createResource<FrameGraphTexture>(name, desc);
			return resource;
		};

		// Albedo
		data.albedo = create_texture(VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_ATTACHMENT | TEXTURE_USAGE_TRANSFER_DST, "GBuffer Albedo Image");
		data.albedo = builder.write(data.albedo);

		// Normal
		data.normal = create_texture(VK_FORMAT_R16G16B16A16_SFLOAT, TEXTURE_USAGE_ATTACHMENT, "GBuffer Normal Image", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		data.normal = builder.write(data.normal);

		// Depth-Stencil
		data.depth = create_texture(VK_FORMAT_D32_SFLOAT_S8_UINT, TEXTURE_USAGE_ATTACHMENT, "GBuffer DepthStencil Image", VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
		data.depth = builder.write(data.depth);

		// Shading
		data.shading = create_texture(VK_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_ATTACHMENT, "GBuffer Shading Image");
		data.shading = builder.write(data.shading);
	},
	[=](const GBufferData &data, const RenderPassResources &resources, CommandBuffer &command_buffer)
	{
		// Render
		auto &albedo = resources.getResource<FrameGraphTexture>(data.albedo);
		auto &normal = resources.getResource<FrameGraphTexture>(data.normal);
		auto &depth = resources.getResource<FrameGraphTexture>(data.depth);
		auto &shading = resources.getResource<FrameGraphTexture>(data.shading);

		VkWrapper::cmdBeginRendering(command_buffer, 
									 {
										 albedo.texture,
										 normal.texture,
										 shading.texture
									 },
									 depth.texture);

		// Render meshes into gbuffer
		auto &p = VkWrapper::global_pipeline;
		p->reset();

		p->setVertexShader(gbuffer_vertex_shader);
		p->setFragmentShader(gbuffer_fragment_shader);

		p->setRenderTargets(VkWrapper::current_render_targets);
		p->setUseBlending(false);

		p->flush();
		p->bind(command_buffer);

		Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

		auto entities_id = Scene::getCurrentScene()->getEntitiesWith<MeshRendererComponent>();
		for (entt::entity entity_id : entities_id)
		{
			Entity entity(entity_id);
			MeshRendererComponent &mesh_renderer_component = entity.getComponent<MeshRendererComponent>();

			entity_renderer->renderEntity(command_buffer, entity);
		}

		p->unbind(command_buffer);
		VkWrapper::cmdEndRendering(command_buffer);
	});
}