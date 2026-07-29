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
    [[vk::location(0)]] float3 pos    : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 normal : NORMAL;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput output;

    float4 finalPos = mul(transformUBO.proj, mul(transformUBO.invView, mul(transformUBO.world, float4(input.pos, 1.0))));
    float4 finalNormal = normalize(mul(transformUBO.world, float4(input.normal, 0.0)));

    output.position = finalPos;
    output.normal = finalNormal.xyz;

    return output;
}
