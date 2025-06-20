#version 450

layout(location = 0) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main()
{
	vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.0));
	vec3 cubeColor = vec3(1.0);

	float lightContrib = clamp(dot(inNormal, lightDir), 0.0, 1.0);
	vec3 ambientLight = vec3(0.1);

	vec3 finalColor = (cubeColor * lightContrib) + ambientLight; 
	outColor = vec4(finalColor, 1.0);
}