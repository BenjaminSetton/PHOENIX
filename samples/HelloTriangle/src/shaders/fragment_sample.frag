#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Test
{
    float time;
} testUBO;

void main()
{
	float normSin = sin(testUBO.time) * 0.5 + 0.5;
	vec4 col = inColor;
	col.g = normSin;
	outColor = col;
}