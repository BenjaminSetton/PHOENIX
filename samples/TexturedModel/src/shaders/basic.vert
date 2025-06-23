#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

layout(set = 0, binding = 0) uniform Transform
{
    mat4 world;
    mat4 invView;
    mat4 proj;
}  transformUBO;

void main()
{
    vec4 finalPos = transformUBO.proj * transformUBO.invView * transformUBO.world * vec4(inPos, 1.0);
    vec4 finalNormal = normalize(transformUBO.world * vec4(inNormal, 0.0f));

    gl_Position =  finalPos;
    outNormal = finalNormal.xyz;
    outUV = inUV;
}