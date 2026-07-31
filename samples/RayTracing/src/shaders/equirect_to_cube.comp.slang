struct CubeFaceSize
{
    uint2 faceSize;
};

[[vk::binding(0, 0)]] Sampler2D equirectMap;
[[vk::binding(1, 0)]] [format("rgba16f")] RWTexture2DArray<float4> cubeMap;
[[vk::binding(2, 0)]] ConstantBuffer<CubeFaceSize> u;

// Convert a direction to equirectangular UV coordinates
float2 directionToEquirectUV(float3 dir)
{
    float uv_x = atan2(dir.z, dir.x) * 0.15915494309189535 + 0.5;
    float uv_y = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) * 0.3183098861837907;
    return float2(uv_x, uv_y);
}

// Convert (face, x, y) to a direction
// Vulkan cubemap face conventions:
// +X: u=-z, v=-y | -X: u=+z, v=-y
// +Y: u=+x, v=+z | -Y: u=+x, v=-z
// +Z: u=+x, v=-y | -Z: u=-x, v=-y
float3 faceCoordToDirection(uint face, float2 uv)
{
    float2 d = uv * 2.0 - 1.0;

    float3 dir;
    switch (face)
    {
    case 0: dir = float3( 1.0,  -d.y,  -d.x); break; // +X
    case 1: dir = float3(-1.0,  -d.y,   d.x); break; // -X
    case 2: dir = float3(  d.x,   1.0,   d.y); break; // +Y
    case 3: dir = float3(  d.x,  -1.0,  -d.y); break; // -Y
    case 4: dir = float3(  d.x,  -d.y,   1.0); break; // +Z
    case 5: dir = float3( -d.x,  -d.y,  -1.0); break; // -Z
    default: dir = float3(0.0); break;
    }
    return normalize(dir);
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 globalID : SV_DispatchThreadID)
{
    uint2 pixel = globalID.xy;
    if (pixel.x >= u.faceSize.x || pixel.y >= u.faceSize.y)
        return;

    uint face = globalID.z;
    if (face >= 6)
        return;

    float2 uv = (float2(pixel) + 0.5) / float2(u.faceSize);
    float3 dir = faceCoordToDirection(face, uv);

    float2 equirectUV = directionToEquirectUV(dir);
    float3 color = equirectMap.SampleLevel(equirectUV, 0).rgb;

    cubeMap[int3(int(pixel.x), int(pixel.y), int(face))] = float4(color, 1.0);
}
