struct RayPayload
{
    float3 hitColor;
    float shadowFactor;
    float3 bounceColor;
};

[[vk::binding(6, 0)]] Sampler2DArray envCubeMap;

// Convert a direction to cubemap face index and UV coordinates.
// This is the inverse of the faceCoordToDirection function used in the equirect-to-cube shader.
// Vulkan cubemap face conventions:
// +X: u=-z, v=-y | -X: u=+z, v=-y
// +Y: u=+x, v=+z | -Y: u=+x, v=-z
// +Z: u=+x, v=-y | -Z: u=-x, v=-y
void directionToFaceUV(float3 dir, out uint face, out float2 uv)
{
    float3 absDir = abs(dir);

    if (absDir.x >= absDir.y && absDir.x >= absDir.z)
    {
        if (dir.x > 0)
        {
            face = 0;
            uv = float2((-dir.z / absDir.x + 1.0) * 0.5, (-dir.y / absDir.x + 1.0) * 0.5);
        }
        else
        {
            face = 1;
            uv = float2((dir.z / absDir.x + 1.0) * 0.5, (-dir.y / absDir.x + 1.0) * 0.5);
        }
    }
    else if (absDir.y >= absDir.x && absDir.y >= absDir.z)
    {
        if (dir.y > 0)
        {
            face = 2;
            uv = float2((dir.x / absDir.y + 1.0) * 0.5, (dir.z / absDir.y + 1.0) * 0.5);
        }
        else
        {
            face = 3;
            uv = float2((dir.x / absDir.y + 1.0) * 0.5, (-dir.z / absDir.y + 1.0) * 0.5);
        }
    }
    else
    {
        if (dir.z > 0)
        {
            face = 4;
            uv = float2((dir.x / absDir.z + 1.0) * 0.5, (-dir.y / absDir.z + 1.0) * 0.5);
        }
        else
        {
            face = 5;
            uv = float2((-dir.x / absDir.z + 1.0) * 0.5, (-dir.y / absDir.z + 1.0) * 0.5);
        }
    }
}

// Miss shader for the indirect bounce ray. This must write to payload.bounceColor
// (not payload.hitColor, which miss.rmiss.slang uses for primary rays) so that the
// sky contributes energy to indirect lighting when a bounce ray escapes to the sky.
[shader("miss")]
void main(inout RayPayload payload)
{
    float3 dir = normalize(WorldRayDirection());
    uint face;
    float2 uv;
    directionToFaceUV(dir, face, uv);
    payload.bounceColor = envCubeMap.SampleLevel(float3(uv, float(face)), 0).rgb;
}
