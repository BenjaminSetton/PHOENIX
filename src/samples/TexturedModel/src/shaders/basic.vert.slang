struct Transform
{
    float4x4 world;
    float4x4 invView;
    float4x4 proj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<Transform> transformUBO;

struct VSInput
{
    [[vk::location(0)]] float3 inPosition  : POSITION;
    [[vk::location(1)]] float3 inNormal    : NORMAL;
    [[vk::location(2)]] float3 inTangent   : TANGENT;
    [[vk::location(3)]] float3 inBitangent : BITANGENT;
    [[vk::location(4)]] float2 inUV        : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3   outWorldPosition : WORLD_POS;
    [[vk::location(1)]] float3   outNormal        : NORMAL;
    [[vk::location(2)]] float2   outUV            : TEXCOORD0;
    [[vk::location(3)]] float3x3 outTBN           : TBN;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    output.position = mul(transformUBO.proj, mul(transformUBO.invView, mul(transformUBO.world, float4(input.inPosition, 1.0))));

    // Calculate the output variables going to the pixel shader
    output.outWorldPosition = mul(transformUBO.world, float4(input.inPosition, 1.0)).xyz;

    output.outNormal = normalize(mul(transformUBO.world, float4(input.inNormal, 0.0)).xyz);
    output.outUV = input.inUV;

    // Construct the TBN matrix
    float3 T = normalize(mul(transformUBO.world, float4(input.inTangent, 0.0)).xyz);
    float3 N = output.outNormal;

    // Re-orthogonalize the TBN matrix in case the tangents are interpolated between vertices (using Gram-Schmidt process)
    T = normalize(T - dot(T, N) * N);

    //float3 B = cross(N, T);
    float3 B = normalize(mul(transformUBO.world, float4(input.inBitangent, 0.0)).xyz);

    // TBN must form a right handed coord system.
    // Some models have symmetric UVs. Check and fix.
    if (dot(cross(N, T), B) < 0.0)
    {
        T = T * -1.0;
    }

    output.outTBN = float3x3(T, B, N);

    return output;
}
