// LOD mesh vertex shader
// Renders instanced LOD meshes. Each draw command (per LOD level) uses firstInstance
// as an offset into the global instance index list. SV_InstanceID is relative to the
// draw command, so the actual instance index is:
//   instanceIndexList[firstInstance + SV_InstanceID]
// This indirection allows the compute shader to compact visible instances per LOD level.
//
// Binding layout (set 0):
//   0: InstanceBuffer (SSBO, read)
//   1: InstanceIndexListBuffer (SSBO, read)
//   2: CameraBuffer (UBO, read)

struct VSInput
{
    [[vk::location(0)]] float3 inPosition : POSITION;
    [[vk::location(1)]] float3 inNormal   : NORMAL;
    [[vk::location(2)]] float2 inUV       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float3 fragNormal : NORMAL;
    [[vk::location(1)]] float2 fragUV     : TEXCOORD0;
    [[vk::location(2)]] uint   fragCulled : CULLED; // 1 = frustum-culled, 0 = visible
};

// Top bit of a stored instance index marks the instance as frustum-culled (see lod_cull.comp).
static const uint INSTANCE_CULLED_FLAG = 0x80000000u;

struct InstanceData
{
    float4x4 modelMatrix;
    uint     groupIndex;
    uint     _pad[3];
};

struct CameraData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4   cameraPos;
    float4   screenParams;
    uint     instanceCount;
    uint     groupCount;
    uint     _pad[2];
};

[[vk::binding(0, 0)]] StructuredBuffer<InstanceData> instances;
[[vk::binding(1, 0)]] StructuredBuffer<uint>         instanceIndexList;
[[vk::binding(2, 0)]] ConstantBuffer<CameraData>    camera;

[shader("vertex")]
VSOutput main(VSInput input, uint instanceIdx : SV_InstanceID, uint drawFirstInstance : SV_StartInstanceLocation)
{
    VSOutput output;

    // Look up the (packed) instance index from the compacted list, then split off the
    // frustum-culled flag stored in the top bit.
    uint packedIdx = instanceIndexList[drawFirstInstance + instanceIdx];
    uint actualInstanceIdx = packedIdx & ~INSTANCE_CULLED_FLAG;
    output.fragCulled = (packedIdx & INSTANCE_CULLED_FLAG) != 0u ? 1u : 0u;
    InstanceData inst = instances[actualInstanceIdx];

    // Row-major mul(M, v)
    float4 worldPos = mul(inst.modelMatrix, float4(input.inPosition, 1.0));
    output.position = mul(camera.projMatrix, mul(camera.viewMatrix, worldPos));

    // Transform normal by upper-left 3x3 of model matrix
    float3x3 upperLeft = float3x3(
        inst.modelMatrix[0].xyz,
        inst.modelMatrix[1].xyz,
        inst.modelMatrix[2].xyz
    );
    output.fragNormal = normalize(mul(upperLeft, input.inNormal));
    output.fragUV = input.inUV;

    return output;
}
