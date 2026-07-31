#include "common.slang"

// Shared payload struct — all ray types use the same struct.
// Individual shaders read/write only the fields they need.
struct RayPayload
{
    float3 hitColor;
    float shadowFactor;
    float3 bounceColor;
};

struct CameraData
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float3 cameraPosition;
    float padding0;
    float2 viewport;
    uint frameCount;
    uint resetAccumulation;
    float2 padding1;
};

[[vk::binding(0, 0)]] [format("rgba16f")] RWTexture2D<float4> outputImage;
[[vk::binding(1, 0)]] RaytracingAccelerationStructure topLevelAS;
[[vk::binding(7, 0)]] [format("rgba16f")] RWTexture2D<float4> accumulationImage;

[[vk::binding(0, 1)]] ConstantBuffer<CameraData> camera;

[shader("raygeneration")]
void main()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dim = DispatchRaysDimensions().xy;

    // Per-pixel random seed based on pixel coords and frame count
    uint seed = pixel.x + pixel.y * dim.x + camera.frameCount * 719393u;
    float2 jitter = RandFloat2(seed) - 0.5;

    float2 uv = (float2(pixel) + 0.5 + jitter) / float2(dim);
    float2 ndc = uv * 2.0 - 1.0;

    // No CPU transpose. Slang's row-major default → SPIR-V ColMajor, so
    // Slang M[i] = GLM column i. mul(v, M) = G * v (correct), mul(M, v) = G^T * v (wrong).
    // ndc.z = -1.0 for OpenGL-style projection (depth range [-1,1], near plane at z=-1)
    float4 target = mul(float4(ndc, -1.0, 1.0), camera.projInverse);
    target.xyz /= target.w;
    float3 direction = normalize(mul(float4(target.xyz, 0.0), camera.viewInverse).xyz);
    float3 origin = camera.cameraPosition;

    RayPayload payload;
    payload.hitColor = float3(0.0);
    payload.shadowFactor = 0.0;
    payload.bounceColor = float3(0.0);

    RayDesc ray;
    ray.Origin = origin;
    ray.TMin = 0.001;
    ray.Direction = direction;
    ray.TMax = 1000.0;

    TraceRay(topLevelAS, 0, 0xff, 0, 0, 0, ray, payload);

    // Clamp to prevent fireflies from high-variance specular samples
    float3 hitColor = min(payload.hitColor, float3(10.0));

    // Progressive accumulation
    if (camera.resetAccumulation == 1u)
    {
        accumulationImage[pixel] = float4(hitColor, 1.0);
    }
    else
    {
        float3 prevAccum = accumulationImage[pixel].rgb;
        float3 newAccum = (prevAccum * float(camera.frameCount) + hitColor) / float(camera.frameCount + 1u);
        accumulationImage[pixel] = float4(newAccum, 1.0);
    }

    // Write the accumulated result to the output image for the blit pass
    float3 finalColor = accumulationImage[pixel].rgb;
    outputImage[pixel] = float4(finalColor, 1.0);
}
