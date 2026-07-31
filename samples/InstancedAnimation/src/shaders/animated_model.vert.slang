struct VSInput
{
    [[vk::location(0)]] float3   inPosition    : POSITION;
    [[vk::location(1)]] float3   inNormal      : NORMAL;
    [[vk::location(2)]] float2   inUV          : TEXCOORD0;
    [[vk::location(3)]] uint4    inBoneIndices : BONEINDICES;
    [[vk::location(4)]] float4   inBoneWeights : BONEWEIGHTS;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 fragNormal : NORMAL;
    [[vk::location(1)]] float2 fragUV     : TEXCOORD0;
};

struct InstanceData
{
    float4x4 modelMatrix;
    float    timeOffset;
    uint     clipIndex;
    uint     _pad[2];
};

[[vk::binding(0, 0)]] StructuredBuffer<float4x4>    boneMatrices;
[[vk::binding(1, 0)]] StructuredBuffer<InstanceData> instances;

struct CameraData
{
    float4x4 view;
    float4x4 proj;
    uint     boneCount;
    uint     _pad[3];
};

[[vk::binding(2, 0)]] ConstantBuffer<CameraData> camera;

[shader("vertex")]
VSOutput main(VSInput input, uint instanceIdx : SV_InstanceID)
{
    VSOutput output;

    // Skin the vertex (row-major mul(M, v) convention)
    float4x4 skinMatrix = float4x4(0.0, 0.0, 0.0, 0.0,
                                   0.0, 0.0, 0.0, 0.0,
                                   0.0, 0.0, 0.0, 0.0,
                                   0.0, 0.0, 0.0, 0.0);
    for (int i = 0; i < 4; i++)
    {
        uint boneIdx = input.inBoneIndices[i];
        float weight = input.inBoneWeights[i];
        if (weight > 0.0)
        {
            skinMatrix = skinMatrix + boneMatrices[instanceIdx * camera.boneCount + boneIdx] * weight;
        }
    }

    float4 skinnedPos = mul(skinMatrix, float4(input.inPosition, 1.0));

    InstanceData inst = instances[instanceIdx];
    float4 worldPos = mul(inst.modelMatrix, skinnedPos);

    output.position = mul(camera.proj, mul(camera.view, worldPos));

    float4x4 skinModel = mul(inst.modelMatrix, skinMatrix);
    float3x3 upperLeft = float3x3(
        skinModel[0].xyz,
        skinModel[1].xyz,
        skinModel[2].xyz
    );
    output.fragNormal = normalize(mul(upperLeft, input.inNormal));
    output.fragUV = input.inUV;

    return output;
}
