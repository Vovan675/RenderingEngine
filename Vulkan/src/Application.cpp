#include "pch.h"
#include "Application.h"
#include "VkWrapper.h"
#include "BindlessResources.h"
#include "Entity.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "imgui.h"

Application::Application()
{
	camera = std::make_shared<Camera>();

	// Load mesh
	//auto entity = std::make_shared<Entity>("assets/barrels/source/Industrial_Updated.fbx");
	auto entity = std::make_shared<Entity>("assets/bistro/BistroExterior.fbx");

	//entity->transform.local_model_matrix = glm::scale(glm::mat4(1), glm::vec3(0.9, 0.9, 0.9));
	//entity->updateTransform();
	//auto mesh = std::make_shared<Engine::Mesh>("assets/bistro/BistroExterior.fbx");
	//auto mesh = std::make_shared<Engine::Mesh>("assets/barrels/source/Industrial_Updated.fbx");
	auto mesh = std::make_shared<Engine::Mesh>("assets/model2.obj");
	auto mesh2 = std::make_shared<Engine::Mesh>("assets/model.fbx");

	//cubemap_renderer = std::make_shared<CubeMapRenderer>(camera);
	entity_renderer = std::make_shared<EntityRenderer>(camera, entity);
	mesh_renderer = std::make_shared<MeshRenderer>(camera, mesh);
	mesh_renderer2 = std::make_shared<MeshRenderer>(camera, mesh2);
	imgui_renderer = std::make_shared<ImGuiRenderer>(window);
	quad_renderer = std::make_shared<QuadRenderer>();

	onSwapchainRecreated(0, 0);

	// Load Textures
	{
		// For example I have textures
		TextureDescription tex_description {};
		tex_description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		tex_description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		tex_description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		auto tex = new Texture(tex_description);
		tex->load("assets/albedo2.png");
		auto tex2 = new Texture(tex_description);
		tex2->load("assets/albedo.png");

		BindlessResources::addTexture(tex);
		BindlessResources::addTexture(tex2);
	}
}

void Application::update(float delta_time)
{
	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	camera->update(delta_time, glm::vec2(mouse_x, mouse_y), glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS);
	imgui_renderer->begin();

	// Draw Imgui Windows
	//ImGui::ShowDemoWindow();
	if (ImGui::Button("Recompile shaders") || ImGui::IsKeyReleased(ImGuiKey_R))
	{
		// Wait for all operations complete
		vkDeviceWaitIdle(VkWrapper::device->logicalHandle);
		mesh_renderer->recreatePipeline();
		mesh_renderer2->recreatePipeline();
		quad_renderer->recreatePipeline();
	}
	
	static int present_mode = 0;
	char* items[] = { "All", "Albedo", "Normal" };
	if (ImGui::BeginCombo("Preview Combo", items[present_mode]))
	{
		for (int n = 0; n < IM_ARRAYSIZE(items); n++)
		{
			bool is_selected = (present_mode == n);
			if (ImGui::Selectable(items[n], is_selected))	
				present_mode = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	quad_renderer->ubo.present_mode = present_mode;

}

void Application::updateBuffers(float delta_time, uint32_t image_index)
{
	// Update bindless resources in any
	BindlessResources::updateSets();

	entity_renderer->updateUniformBuffer(image_index);

	mesh_renderer->setRotation(glm::rotate(glm::quat(), glm::vec3(-glm::radians(90.0f), -glm::radians(90.0f), 0)));
	mesh_renderer->setPosition(glm::vec3(0, 0, 0));
	mesh_renderer->setScale(glm::vec3(1));
	mesh_renderer->updateUniformBuffer(image_index);

	double time = glfwGetTime();
	mesh_renderer2->setPosition(glm::vec3(1, 0.3, 0));
	mesh_renderer2->setScale(glm::vec3(0.02f, 0.02f, 0.02f));
	mesh_renderer2->updateUniformBuffer(image_index);
	//cubemap_renderer->updateUniformBuffer(image_index);

	Material mat1;
	mat1.albedo_tex_id = 2;
	Material mat2;
	mat2.albedo_tex_id = 2;
	mesh_renderer->setMaterial(mat1);
	mesh_renderer2->setMaterial(mat2);

	quad_renderer->updateUniformBuffer(image_index);
}

void Application::recordCommands(CommandBuffer &command_buffer, uint32_t image_index)
{
	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
									 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									 gbuffer_albedo->imageHandle, VK_IMAGE_ASPECT_COLOR_BIT);
	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
									 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									 gbuffer_normal->imageHandle, VK_IMAGE_ASPECT_COLOR_BIT);
	// Begin rendering to gbuffer
	{
		std::vector<std::shared_ptr<Texture>> color_attachments = {gbuffer_albedo, gbuffer_normal};
		VkWrapper::cmdBeginRendering(command_buffer, color_attachments, depth_stencil_textures[image_index]);
	}

	entity_renderer->fillCommandBuffer(command_buffer, image_index);

	// Render mesh into gbuffer
	mesh_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);
	// End gbuffer rendering

	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
									 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
									 gbuffer_albedo->imageHandle, VK_IMAGE_ASPECT_COLOR_BIT);
	VkWrapper::cmdImageMemoryBarrier(command_buffer,
									 VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
									 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
									 gbuffer_normal->imageHandle, VK_IMAGE_ASPECT_COLOR_BIT);

	// Begin rendering lighting
	// TODO: render lights to another texture
	// End lighting rendering

	// Begin rendering to present (just output texture)
	{
		std::vector<std::shared_ptr<Texture>> color_attachments = {VkWrapper::swapchain->swapchain_textures[image_index]};
		VkWrapper::cmdBeginRendering(command_buffer, color_attachments, nullptr);
	}

	// Render quad
	quad_renderer->fillCommandBuffer(command_buffer, image_index);

	// Render imgui
	imgui_renderer->fillCommandBuffer(command_buffer, image_index);

	VkWrapper::cmdEndRendering(command_buffer);

	{
		VkImageMemoryBarrier2 image_memory_barrier{};
		image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		image_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_memory_barrier.srcAccessMask = 0;
		image_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_memory_barrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		image_memory_barrier.image = gbuffer_albedo->imageHandle;
		image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		image_memory_barrier.subresourceRange.baseMipLevel = 0;
		image_memory_barrier.subresourceRange.levelCount = 1;
		image_memory_barrier.subresourceRange.baseArrayLayer = 0;
		image_memory_barrier.subresourceRange.layerCount = 1;

		VkDependencyInfo dependency_info{};
		dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependency_info.imageMemoryBarrierCount = 1;
		dependency_info.pImageMemoryBarriers = &image_memory_barrier;

		//vkCmdPipelineBarrier2(command_buffer.get_buffer(), &dependency_info);
	}
}

void Application::cleanupResources()
{
	gbuffer_albedo = nullptr;
	gbuffer_normal = nullptr;
	depth_stencil_textures.clear();
	entity_renderer = nullptr;
	mesh_renderer = nullptr;
	mesh_renderer2 = nullptr;
	imgui_renderer = nullptr;
	quad_renderer = nullptr;
}

void Application::onSwapchainRecreated(int width, int height)
{
	depth_stencil_textures.resize(VkWrapper::swapchain->swapchain_images.size());
	for (int i = 0; i < VkWrapper::swapchain->swapchain_images.size(); i++)
	{
		TextureDescription description;
		description.width = VkWrapper::swapchain->swap_extent.width;
		description.height = VkWrapper::swapchain->swap_extent.height;
		description.mipLevels = 1;
		description.numSamples = VK_SAMPLE_COUNT_1_BIT;
		description.imageFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
		description.imageAspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
		description.imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depth_stencil_textures[i] = std::make_shared<Texture>(description);
		depth_stencil_textures[i]->fill();
	}

	// init gbuffer resources
	{
		TextureDescription description;
		description.width = VkWrapper::swapchain->swap_extent.width;
		description.height = VkWrapper::swapchain->swap_extent.height;
		description.mipLevels = 1;
		description.numSamples = VK_SAMPLE_COUNT_1_BIT;
		description.imageFormat = VkWrapper::swapchain->surface_format.format;
		description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		gbuffer_albedo = std::make_shared<Texture>(description);
		gbuffer_albedo->fill();

		uint32_t albedo_id = BindlessResources::addTexture(gbuffer_albedo.get());
		quad_renderer->ubo.albedo_tex_id = albedo_id;
	}
	{
		TextureDescription description;
		description.width = VkWrapper::swapchain->swap_extent.width;
		description.height = VkWrapper::swapchain->swap_extent.height;
		description.mipLevels = 1;
		description.numSamples = VK_SAMPLE_COUNT_1_BIT;
		description.imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		description.imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		description.imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		gbuffer_normal = std::make_shared<Texture>(description);
		gbuffer_normal->fill();

		uint32_t normal_id = BindlessResources::addTexture(gbuffer_normal.get());
		quad_renderer->ubo.normal_tex_id = normal_id;
	}
}

void Application::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	bool is_pressed = action != GLFW_RELEASE;
	if (key == GLFW_KEY_W)
		camera->inputs.forward = is_pressed;
	if (key == GLFW_KEY_S)
		camera->inputs.backward = is_pressed;
	if (key == GLFW_KEY_A)
		camera->inputs.left = is_pressed;
	if (key == GLFW_KEY_D)
		camera->inputs.right = is_pressed;
}

