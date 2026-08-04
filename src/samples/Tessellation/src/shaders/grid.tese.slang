struct HSOutput
{
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD0;
};

struct PatchConst
{
    float edges[4]  : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
};

struct DSOutput
{
    float4 position    : SV_Position;
    float3 worldPos    : POSITION;
    float2 uv          : TEXCOORD0;
    float  tessFactor  : TEXCOORD1;
};

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float3   camPos;
    float    _pad;
};

struct TessParams
{
    float maxTessFactor;
    float minDistance;
    float maxDistance;
    float displacementScale;
};

[[vk::binding(0, 0)]] ConstantBuffer<CameraData>  camera;
[[vk::binding(1, 0)]] ConstantBuffer<TessParams>  tessParams;

// Sine-wave heightmap displacement
float GetHeight(float2 uv)
{
    return sin(uv.x * 8.0) * cos(uv.y * 8.0) * tessParams.displacementScale;
}

[shader("domain")]
[domain("quad")]
DSOutput main(
    PatchConst pc,
    OutputPatch<HSOutput, 4> patch,
    float2 uv : SV_DomainLocation)
{
    DSOutput output;

    // Bilinear interpolation of the quad control points.
    // Vertex layout from C++: patch[0]=BL, patch[1]=BR, patch[2]=TR, patch[3]=TL
    // For uv=(u,v): BL*(1-u)*(1-v) + BR*u*(1-v) + TR*u*v + TL*(1-u)*v
    float3 worldPos =
          patch[0].worldPos * (1.0 - uv.x) * (1.0 - uv.y)  // BL
        + patch[1].worldPos * uv.x       * (1.0 - uv.y)    // BR
        + patch[2].worldPos * uv.x       * uv.y            // TR
        + patch[3].worldPos * (1.0 - uv.x) * uv.y;         // TL

    float2 interpolatedUV =
          patch[0].uv * (1.0 - uv.x) * (1.0 - uv.y)
        + patch[1].uv * uv.x       * (1.0 - uv.y)
        + patch[2].uv * uv.x       * uv.y
        + patch[3].uv * (1.0 - uv.x) * uv.y;

    // Apply heightmap displacement
    worldPos.y += GetHeight(interpolatedUV);

    // Transform to clip space (matrices are transposed on C++ side for row-major Slang)
    // Using mul(M, v) convention as seen in existing samples
    float4 viewPos = mul(camera.view, float4(worldPos, 1.0));
    output.position = mul(camera.proj, viewPos);
    output.worldPos = worldPos;
    output.uv = interpolatedUV;
    output.tessFactor = pc.edges[0];

    return output;
}
