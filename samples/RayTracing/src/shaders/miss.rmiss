#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 6) uniform samplerCube envCubeMap;

layout(location = 0) rayPayloadInEXT vec3 hitColor;

void main()
{
    vec3 dir = gl_WorldRayDirectionEXT;
    hitColor = texture(envCubeMap, dir).rgb;
}
