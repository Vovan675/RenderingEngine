#include "common.h"

layout (location = 0) in vec4 inPos;

layout(push_constant) uniform constants
{
	mat4 model;
};

layout (binding = 0) uniform UBO 
{
	mat4 light_space_matrix;
	vec4 light_pos;
	float z_far;
	float padding_1;
	float padding_2;
	float padding_3;
};

layout (location = 0) out vec4 outPos;

void main() 
{
	gl_Position = light_space_matrix * model * inPos;
	outPos = model * inPos;
}
