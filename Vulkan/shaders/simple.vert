layout(set=0, binding=0) uniform UniformBufferObject
{
	mat4 model;
	mat4 view;
	mat4 proj;
	vec3 cameraPosition;
} ubo;

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec3 inTangent;
layout(location=3) in vec2 inUV;
layout(location=4) in vec3 inColor;

layout(location=0) out vec3 fragPos;
layout(location=1) out vec3 fragNormal;
layout(location=2) out vec2 fragUV;
layout(location=3) out vec3 fragColor;

void main()
{
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
	fragPos = vec3(ubo.model * vec4(inPosition, 1.0));
	fragNormal = normalize(mat3(ubo.model) * inNormal); // Apply rotation to normal
	fragUV = inUV;
	fragColor = inColor;
}