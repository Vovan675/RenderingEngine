layout (location=0) in vec3 dir;
layout (location=0) out vec4 outColor;

layout (binding=1) uniform samplerCube texSampler;

layout (set=1, binding=0) uniform sampler2D textures[];

const float PI = 3.14159265359;
const float TwoPI = 2 * PI;

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

vec3 sampleHemisphere(float u1, float u2)
{
	const float u1p = sqrt(max(0.0, 1.0 - u1 * u1));
	return vec3(cos(TwoPI * u2) * u1p, sin(TwoPI * u2) * u1p, u1);
}

void main()
{
	vec3 N = normalize(dir);

	vec3 up = vec3(0.0, 1.0, 0.0);
	vec3 right = normalize(cross(up, N));
	up = normalize(cross(N, right));

	vec3 irradiance = vec3(0.0);
	int samples_count = 0;

	#if 1
		float step_phi = (2.0 * PI) / 180.0;
		float step_theta = (0.5 * PI) / 64.0;

		for(float phi = 0.0f; phi < 2.0 * PI; phi += step_phi)
		{
			for(float theta = 0.0f; theta < 0.5 * PI; theta += step_theta)
			{
				// Convert spherical coordinate to cartesian (local space, aka tangent space)
				vec3 sample_tangent = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
				vec3 sample_dir = sample_tangent.x * right + sample_tangent.y * up + sample_tangent.z * N;
				irradiance += texture(texSampler, sample_dir).rgb * cos(theta) * sin(theta);
				samples_count++;
			}
		}
		outColor = vec4(PI * irradiance / float(samples_count), 1.0);
	#else
		samples_count = 64;
		for (int i = 0; i < samples_count; i++)
		{
        	vec2 Xi = Hammersley(i, samples_count);
			vec3 sample_tangent = sampleHemisphere(Xi.x, Xi.y);
			vec3 sample_dir = sample_tangent.x * right + sample_tangent.y * up + sample_tangent.z * N;
			float cosTheta = max(0.0, dot(N, sample_dir));
			irradiance += texture(texSampler, sample_dir).rgb * cosTheta;
		}
		outColor = vec4(PI * irradiance / float(samples_count), 1.0);
	#endif

}