#include "pch.h"
#include "DebugRenderer.h"
#include "BindlessResources.h"
#include "Rendering/Renderer.h"

DebugRenderer::DebugRenderer()
{
	vertex_shader_lines = Shader::create("shaders/debug_lines.vert", Shader::VERTEX_SHADER);
	fragment_shader_lines = Shader::create("shaders/debug_lines.frag", Shader::FRAGMENT_SHADER);

	uint32_t indices[256];
	for (size_t i = 0; i < 256; i++)
	{
		indices[i] = i;
	}

	BufferDescription desc;
	desc.size = sizeof(uint32_t) * 256;
	desc.useStagingBuffer = true;
	desc.bufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	lines_index_buffer = Buffer::create(desc);
	lines_index_buffer->fill(indices);
	lines_index_buffer->setDebugName("Lines Index Buffer");

	vertices = new Engine::Vertex[256];
	
	desc.size = sizeof(Engine::Vertex) * 256;
	desc.useStagingBuffer = false;
	desc.bufferUsageFlags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	lines_vertex_buffer = Buffer::create(desc);
	lines_vertex_buffer->map((void **)&vertices);
	lines_vertex_buffer->setDebugName("Lines Vertex Buffer");
}

DebugRenderer::~DebugRenderer()
{
}

void DebugRenderer::fillCommandBuffer(CommandBuffer &command_buffer)
{
	ubo.albedo_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_ALBEDO);
	ubo.shading_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_SHADING);
	ubo.normal_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_NORMAL);
	ubo.depth_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_GBUFFER_DEPTH_STENCIL);
	ubo.light_diffuse_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_DIFFUSE);
	ubo.light_specular_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_LIGHTING_SPECULAR);
	ubo.brdf_lut_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_BRDF_LUT);
	ubo.ssao_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_SSAO);
	ubo.composite_final_tex_id = Renderer::getRenderTargetBindlessId(RENDER_TARGET_COMPOSITE);

	auto &p = VkWrapper::global_pipeline;
	p->bindScreenQuadPipeline(command_buffer, Shader::create("shaders/debug_quad.frag", Shader::FRAGMENT_SHADER));

	// Uniforms
	Renderer::setShadersUniformBuffer(p->getCurrentShaders(), 0, &ubo, sizeof(PresentUBO));
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	// Render quad
	vkCmdDraw(command_buffer.get_buffer(), 6, 1, 0, 0);

	p->unbind(command_buffer);
}

void DebugRenderer::renderLines(CommandBuffer &command_buffer)
{
	auto &p = VkWrapper::global_pipeline;
	p->reset();

	p->setVertexShader(vertex_shader_lines);
	p->setFragmentShader(fragment_shader_lines);

	p->setRenderTargets(VkWrapper::current_render_targets);
	p->setUseBlending(false);
	p->setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
	p->setDepthTest(false);
	p->setCullMode(VK_CULL_MODE_NONE);

	p->flush();
	p->bind(command_buffer);

	// Uniforms
	Renderer::bindShadersDescriptorSets(p->getCurrentShaders(), command_buffer, p->getPipelineLayout());

	VkBuffer vertexBuffers[] = {lines_vertex_buffer->bufferHandle};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(command_buffer.get_buffer(), 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(command_buffer.get_buffer(), lines_index_buffer->bufferHandle, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(command_buffer.get_buffer(), lines_index_count, 1, 0, 0, 0);

	p->unbind(command_buffer);

	lines_index_count = 0;
}

void DebugRenderer::addBoundBox(BoundBox bbox)
{
	addLine(bbox.min, glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z));
	addLine(bbox.min, glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z));
	addLine(bbox.min, glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z));

	addLine(bbox.max, glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
	addLine(bbox.max, glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));
	addLine(bbox.max, glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z));

	addLine(glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
	addLine(glm::vec3(bbox.min.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z));

	addLine(glm::vec3(bbox.max.x, bbox.max.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z));
	addLine(glm::vec3(bbox.max.x, bbox.min.y, bbox.min.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));

	addLine(glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z), glm::vec3(bbox.max.x, bbox.min.y, bbox.max.z));
	addLine(glm::vec3(bbox.min.x, bbox.min.y, bbox.max.z), glm::vec3(bbox.min.x, bbox.max.y, bbox.max.z));
}

std::vector<glm::vec3> DebugRenderer::addCirlce(glm::vec3 center, glm::vec3 normal, float radius, int segments)
{
	std::vector<glm::vec3> circle_points;
	glm::vec3 tangent1, tangent2;

	// Create tangents based on the normal
	if (fabs(normal.x) > fabs(normal.y))
	{
		tangent1 = glm::normalize(glm::vec3(normal.z, 0, -normal.x));
	} else
	{
		tangent1 = glm::normalize(glm::vec3(0, -normal.z, normal.y));
	}
	tangent2 = glm::cross(normal, tangent1);

	for (int i = 0; i < segments; ++i)
	{
		float theta = glm::two_pi<float>() * float(i) / float(segments);
		glm::vec3 circle_point = center + radius * (cos(theta) * tangent1 + sin(theta) * tangent2);
		circle_points.push_back(circle_point);
	}

	// Add lines to draw the circle
	for (int i = 0; i < segments; ++i)
	{
		addLine(circle_points[i], circle_points[(i + 1) % segments]);
	}

	return circle_points;
}

void DebugRenderer::addArrow(glm::vec3 p0, glm::vec3 p1, float arrow_size)
{
	// Add the main line from p0 to p1
	addLine(p0, p1);

	// Calculate the direction from p0 to p1
	glm::vec3 direction = glm::normalize(p1 - p0);

	// Calculate circle at the base of the arrow
	float circle_radius = arrow_size * 0.5f; // Adjust circle radius as needed
	std::vector<glm::vec3> circle_points = addCirlce(p1 - direction * arrow_size, direction, circle_radius, 12);

	// Add lines from the circle to the arrow tip
	for (const auto &point : circle_points)
	{
		addLine(point, p1);
	}

	// Calculate perpendicular vectors to create the arrowhead
	glm::vec3 perpendicular1 = glm::normalize(glm::cross(direction, glm::vec3(0, 1, 0)));
	glm::vec3 perpendicular2 = glm::normalize(glm::cross(direction, perpendicular1));

	glm::vec3 arrowhead_point1 = p1 - direction * arrow_size + perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point2 = p1 - direction * arrow_size - perpendicular1 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point3 = p1 - direction * arrow_size + perpendicular2 * (arrow_size / 2.0f);
	glm::vec3 arrowhead_point4 = p1 - direction * arrow_size - perpendicular2 * (arrow_size / 2.0f);

	// Add the arrowhead lines
	addLine(p1, arrowhead_point1);
	addLine(p1, arrowhead_point2);
	addLine(p1, arrowhead_point3);
	addLine(p1, arrowhead_point4);
}
