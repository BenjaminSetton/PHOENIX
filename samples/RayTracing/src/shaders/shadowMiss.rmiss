#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT float shadowFactor;

void main()
{
    // Ray didn't hit anything — the point is not in shadow
    shadowFactor = 1.0;
}
