struct VSOutput
{
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD0;
};

struct HSOutput
{
    float3 worldPos : POSITION;
    float2 uv       : TEXCOORD0;
};

// Quad patch: 4 outer edges, 2 inner tessellation factors
struct PatchConst
{
    float edges[4]  : SV_TessFactor;
    float inside[2] : SV_InsideTessFactor;
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

// Compute tessellation factor based on distance from camera to patch center.
// Close patches get maxTessFactor, far patches get 1.
float ComputeTessFactor(float3 patchCenter)
{
    float dist = distance(camera.camPos, patchCenter);
    float t = saturate((dist - tessParams.minDistance) / (tessParams.maxDistance - tessParams.minDistance));
    // t=0 at minDistance (close), t=1 at maxDistance (far)
    // Smoothstep for a nicer falloff
    t = t * t * (3.0 - 2.0 * t);
    return lerp(tessParams.maxTessFactor, 1.0, t);
}

PatchConst PatchConstantFunction(InputPatch<VSOutput, 4> patch)
{
    // Patch center is the average of the 4 control points
    float3 center = (patch[0].worldPos + patch[1].worldPos + patch[2].worldPos + patch[3].worldPos) * 0.25;

    float tf = ComputeTessFactor(center);

    PatchConst output;
    output.edges[0] = tf;
    output.edges[1] = tf;
    output.edges[2] = tf;
    output.edges[3] = tf;
    output.inside[0] = tf;
    output.inside[1] = tf;
    return output;
}

[shader("hull")]
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantFunction")]
HSOutput main(InputPatch<VSOutput, 4> patch, uint cpId : SV_OutputControlPointID)
{
    HSOutput output;
    output.worldPos = patch[cpId].worldPos;
    output.uv = patch[cpId].uv;
    return output;
}
