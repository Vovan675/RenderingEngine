#pragma once
#include <spirv_cross/spirv_cross_c.h>

enum DescriptorStage : uint32_t
{
	// Default pipeline
	DESCRIPTOR_STAGE_VERTEX_SHADER = 2 << 0,
	DESCRIPTOR_STAGE_FRAGMENT_SHADER = 2 << 1,
	// Compute pipeline
	DESCRIPTOR_STAGE_COMPUTE_SHADER = 2 << 2,
	// Ray tracing pipeline
	DESCRIPTOR_STAGE_RAY_GENERATION_SHADER = 2 << 3,
	DESCRIPTOR_STAGE_MISS_SHADER = 2 << 4,
	DESCRIPTOR_STAGE_CLOSEST_HIT_SHADER = 2 << 5,
};

enum DescriptorType
{
	DESCRIPTOR_TYPE_PUSH_CONSTANT,
	DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	DESCRIPTOR_TYPE_STORAGE_BUFFER,
	DESCRIPTOR_TYPE_SAMPLER,
	DESCRIPTOR_TYPE_STORAGE_IMAGE,
	DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE,
};

struct Descriptor
{
	Descriptor(DescriptorType type, unsigned int set, unsigned int binding, size_t size, DescriptorStage stage, std::string name = "")
		: type(type), set(set), binding(binding), size(size), stage(stage), name(name)
	{

	}

	DescriptorType type;
	unsigned int set;
	unsigned int binding;
	size_t size;
	DescriptorStage stage;
	std::string name;

	unsigned int first_member_offset = 0;
};

class Shader
{
public:
	virtual ~Shader();

	void shutdown();

	VkShaderModule handle = nullptr;
	enum ShaderType
	{
		// Default pipeline
		VERTEX_SHADER,
		FRAGMENT_SHADER,
		// Compute pipeline
		COMPUTE_SHADER,
		// Ray tracing pipeline
		RAY_GENERATION_SHADER,
		MISS_SHADER,
		CLOSEST_HIT_SHADER,
	};

	size_t getHash() const;

	const std::vector<Descriptor> &getDescriptors() const { return descriptors; }

	void recompile();
	static void recompileAllShaders();
	static void destroyAllShaders();

	static std::shared_ptr<Shader> create(const std::string &path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines = {});
private:
	Shader(const std::string &path, ShaderType type, std::vector<std::pair<const char *, const char *>> defines);
private:
	static std::string read_file(const std::string& fileName);
	std::vector<uint32_t> compile_glsl(const std::string& glslSource, ShaderType type, const std::string debugName) const;
	void init_descriptors(const std::vector<uint32_t>& spirv);
	void parse_spv_resources(const spvc_reflected_resource *resources, size_t count, spvc_resource_type resource_type);
private:
	ShaderType type;
	std::string path;
	std::string source;
	std::vector<std::pair<const char *, const char *>> defines;

	std::vector<Descriptor> descriptors;

	size_t hash;
	static std::unordered_map<size_t, std::shared_ptr<Shader>> cached_shaders;

	spvc_compiler compiler;
};

