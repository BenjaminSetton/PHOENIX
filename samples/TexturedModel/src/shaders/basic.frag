#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;

layout(set = 0, binding = 0) uniform sampler2D diffuseSampler;
layout(set = 0, binding = 1) uniform sampler2D normalSampler;
layout(set = 0, binding = 2) uniform sampler2D metallicSampler;
layout(set = 0, binding = 3) uniform sampler2D roughnessSampler;
layout(set = 0, binding = 4) uniform sampler2D lightmapSampler;

layout(location = 0) out vec4 outColor;

void main()
{
	//vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.0));
	//vec3 cubeColor = vec3(1.0);
	//
	//float lightContrib = clamp(dot(inNormal, lightDir), 0.0, 1.0);
	//vec3 ambientLight = vec3(0.1);
	//
	//vec3 finalColor = (cubeColor * lightContrib) + ambientLight; 

	outColor = vec4(inUV, 0.0, 1.0);
}