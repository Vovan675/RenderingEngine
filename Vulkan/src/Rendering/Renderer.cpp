#include "pch.h"
#include "Renderer.h"
#include "BindlessResources.h"


RendererDebugInfo Renderer::prev_debug_info = {};
RendererDebugInfo Renderer::debug_info = {};
std::array<std::shared_ptr<RHITexture>, RENDER_TARGET_TEXTURES_COUNT> Renderer::screen_resources;

Renderer::DefaultUniforms Renderer::default_uniforms;
std::shared_ptr<Camera> Renderer::camera;

std::array<std::vector<std::pair<RESOURCE_TYPE, void *>>, MAX_FRAMES_IN_FLIGHT> Renderer::deletion_queue;
int Renderer::current_frame_in_flight = 0;
int Renderer::current_image_index = 0;
uint64_t Renderer::current_frame = 0;
uint32_t Renderer::timestamp_index = 0;

glm::ivec2 Renderer::viewport_size;

void Renderer::init()
{
	viewport_size = gDynamicRHI->getSwapchainTexture(0)->getSize();
	recreateScreenResources();
}

void Renderer::shutdown()
{
	for (size_t i = 0; i < RENDER_TARGET_TEXTURES_COUNT; i++)
		screen_resources[i] = nullptr;

	//deleteResources(current_frame_in_flight);
	//Renderer::endFrame(0);
}

void Renderer::recreateScreenResources()
{

}

void Renderer::setViewportSize(glm::ivec2 size)
{
	if (viewport_size == size)
		return;
	viewport_size = size;
	recreateScreenResources();
}

void Renderer::beginFrame(unsigned int current_frame_in_flight, unsigned int current_image_index)
{
	current_frame++;
	Renderer::current_frame_in_flight = current_frame_in_flight;
	Renderer::current_image_index = current_image_index;

	// Update debug info
	prev_debug_info = debug_info;
	debug_info = RendererDebugInfo{};

	timestamp_index = 0;

	//deleteResources(current_frame_in_flight);
}

void Renderer::endFrame(unsigned int image_index)
{
	// Reset offsets for uniform buffers
	
	int count = timestamp_index;
	//if (count > 0)
	//	vkGetQueryPoolResults(VkWrapper::device->logicalHandle, VkWrapper::device->query_pools[current_frame_in_flight], 0, count, VkWrapper::device->time_stamps[current_frame_in_flight].size() * sizeof(uint64_t), VkWrapper::device->time_stamps[current_frame_in_flight].data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
}

void Renderer::deleteResources(unsigned int frame_in_flight)
{
	if (!deletion_queue[frame_in_flight].empty())
	{
		/*
		vkDeviceWaitIdle(VkWrapper::device->logicalHandle);

		for (auto resource : deletion_queue[frame_in_flight])
		{
			switch (resource.first)
			{
				case RESOURCE_TYPE_SAMPLER: 
					vkDestroySampler(VkWrapper::device->logicalHandle, (VkSampler)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE_VIEW:
					vkDestroyImageView(VkWrapper::device->logicalHandle, (VkImageView)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_IMAGE:
					vkDestroyImage(VkWrapper::device->logicalHandle, (VkImage)resource.second, nullptr);
					break;
				case RESOURCE_TYPE_MEMORY:
					vmaFreeMemory(VkWrapper::allocator, (VmaAllocation)resource.second);
					break;
				case RESOURCE_TYPE_BUFFER:
					vkDestroyBuffer(VkWrapper::device->logicalHandle, (VkBuffer)resource.second, nullptr);
					break;
			}
		}

		deletion_queue[frame_in_flight].clear();
		*/
	}
}

uint32_t Renderer::beginTimestamp()
{
	uint32_t current_timestep = timestamp_index;
	//vkCmdWriteTimestamp2(getCurrentCommandBuffer().get_buffer(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VkWrapper::device->query_pools[current_frame_in_flight], timestamp_index++);
	timestamp_index++;
	return current_timestep;
}

void Renderer::endTimestamp(uint32_t index)
{
	//vkCmdWriteTimestamp2(getCurrentCommandBuffer().get_buffer(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VkWrapper::device->query_pools[current_frame_in_flight], index);
}

float Renderer::getTimestampTime(uint32_t index)
{
	//float period = VkWrapper::device->physicalProperties.properties.limits.timestampPeriod;
	//float delta_in_ms = (VkWrapper::device->time_stamps[current_frame_in_flight][index + 1] - VkWrapper::device->time_stamps[current_frame_in_flight][index]) * period / 1000000.0f;
	//return delta_in_ms;
	return 0;
}

void Renderer::setShadersAccelerationStructure(std::vector<std::shared_ptr<RHIShader>> shaders, VkAccelerationStructureKHR *acceleration_structure, unsigned int binding)
{
	// TODO:
	//auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	//size_t descriptor_hash = descriptor_layout.hash;
	//// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	//hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	//size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	//ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	//VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	//// Update set
	//DescriptorWriter writer;
	//writer.writeAccelerationStructure(binding, acceleration_structure);
	//writer.updateSet(descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset]);
}

void Renderer::setShadersStorageBuffer(std::vector<std::shared_ptr<RHIShader>> shaders, unsigned int binding, void *params_struct, size_t params_size)
{
	/*
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	size_t binding_hash = descriptor_hash;
	hash_combine(binding_hash, binding);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_binding_frame = descriptor_bindings[binding_hash][i];
		// If there is no descriptor binding for this offset, then create buffer for it and map
		if (current_binding_frame.bindings_per_offset.size() <= offset)
		{
			current_binding_frame.bindings_per_offset.resize(offset + 1);
			auto &current_binding = current_binding_frame.bindings_per_offset[offset];

			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			current_binding.first = Buffer::create(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, current_binding.first->bufferHandle, params_size);
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[binding_hash][current_frame_in_flight].bindings_per_offset[offset];
	memcpy(current_binding.second, params_struct, params_size);
	*/
}

void Renderer::setShadersStorageBuffer(std::vector<std::shared_ptr<RHIShader>> shaders, unsigned int binding, std::shared_ptr<RHIBuffer> buffer)
{
	/*
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	size_t binding_hash = descriptor_hash;
	hash_combine(binding_hash, binding);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_binding_frame = descriptor_bindings[binding_hash][i];
		// If there is no descriptor binding for this offset, then create buffer for it and map
		if (current_binding_frame.bindings_per_offset.size() <= offset)
		{
			current_binding_frame.bindings_per_offset.resize(offset + 1);
			auto &current_binding = current_binding_frame.bindings_per_offset[offset];
			current_binding.first = buffer;

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, current_binding.first->bufferHandle, buffer->getSize());
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}
	*/
}

void Renderer::setShadersUniformBuffer(std::vector<std::shared_ptr<RHIShader>> shaders, unsigned int binding, void *params_struct, size_t params_size)
{
	/*
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));

	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	size_t binding_hash = descriptor_hash;
	hash_combine(binding_hash, binding);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto &current_binding_frame = descriptor_bindings[binding_hash][i];
		// If there is no descriptor binding for this offset, then create buffer for it and map
		// TODO: create buffers with shaders?
		if (current_binding_frame.bindings_per_offset.size() <= offset)
		{
			current_binding_frame.bindings_per_offset.resize(offset + 1);
			auto &current_binding = current_binding_frame.bindings_per_offset[offset];

			BufferDescription desc;
			desc.size = params_size;
			desc.useStagingBuffer = false;
			desc.bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

			current_binding.first = Buffer::create(desc);

			// Map gpu memory on cpu memory
			current_binding.first->map(&current_binding.second);

			// Update set
			DescriptorWriter writer;
			writer.writeBuffer(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, current_binding.first->bufferHandle, params_size);
			writer.updateSet(descriptors[descriptor_hash][i].descriptor_per_offset[offset]);
		}
	}

	auto &current_binding = descriptor_bindings[binding_hash][current_frame_in_flight].bindings_per_offset[offset];
	memcpy(current_binding.second, params_struct, params_size);
	*/
}

void Renderer::setShadersTexture(std::vector<std::shared_ptr<RHIShader>> shaders, unsigned int binding, std::shared_ptr<RHITexture> texture, int mip, int face, bool is_uav)
{
	// TODO: fix
	/*
	auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	size_t descriptor_hash = descriptor_layout.hash;
	// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	ensureDescriptorsAllocated(descriptor_layout, descriptor_hash, offset);

	VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	VkImageLayout image_layout = texture->isDepthTexture() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	if (is_uav && (texture->getUsageFlags() & TEXTURE_USAGE_STORAGE))
	{
		descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		image_layout = VK_IMAGE_LAYOUT_GENERAL;
	}

	// Update set
	DescriptorWriter writer;
	writer.writeImage(binding, descriptor_type, texture->getImageView(mip, face), texture->getSampler(), image_layout);
	writer.updateSet(descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset]);
	*/
}

void Renderer::bindShadersDescriptorSets(std::vector<std::shared_ptr<RHIShader>> shaders, RHICommandList *cmd_list, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point)
{
	// TODO:
	//// Bind bindless
	//vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 1, 1, BindlessResources::getDescriptorSet(), 0, nullptr);

	//// Bind default uniforms
	//size_t default_uniforms_offset = descriptors_offset[0][current_frame_in_flight];
	//vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 2, 1, &descriptors[0][current_frame_in_flight].descriptor_per_offset[default_uniforms_offset], 0, nullptr);

	//// Bind custom descriptor
	//auto descriptor_layout = VkWrapper::getDescriptorLayout(shaders);

	//size_t descriptor_hash = descriptor_layout.hash;
	//// Must also take shaders hashes into account because there could be the same descriptor layout but bindings have different sizes
	//hash_combine(descriptor_hash, VkWrapper::getShadersHash(shaders));
	//size_t offset = descriptors_offset[descriptor_hash][current_frame_in_flight];

	//if (descriptors.find(descriptor_hash) != descriptors.end())
	//{
	//	vkCmdBindDescriptorSets(command_buffer.get_buffer(), bind_point, pipeline_layout, 0, 1, &descriptors[descriptor_hash][current_frame_in_flight].descriptor_per_offset[offset], 0, nullptr);
	//}

	//// After every bind increment offset (because bind was made for a draw that uses this uniforms and we cant write to it)
	//descriptors_offset[descriptor_hash][current_frame_in_flight] += 1;
}

void Renderer::updateDefaultUniforms(float delta_time)
{
	default_uniforms.view = camera->getView();
	default_uniforms.iview = glm::inverse(camera->getView());
	default_uniforms.projection = camera->getProj();
	default_uniforms.iprojection = glm::inverse(camera->getProj());
	default_uniforms.camera_position = glm::vec4(camera->getPosition(), 1.0);
	default_uniforms.swapchain_size = glm::vec4(Renderer::getViewportSize(), 1.0f / glm::vec2(Renderer::getViewportSize()));
	default_uniforms.time += delta_time;
	default_uniforms.frame = (uint32_t)current_frame;
}

const Renderer::DefaultUniforms Renderer::getDefaultUniforms()
{
	return default_uniforms;
}
