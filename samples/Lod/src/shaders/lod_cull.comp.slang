#include "common.slang"

// LOD cull + select compute shader
// One thread per instance. Each thread:
//   1. Frustum-tests the instance against the camera (result kept as a flag, NOT used to skip)
//   2. Selects the appropriate LOD level based on screen-space ratio
//   3. Atomically appends the instance index to that LOD level's compacted instance list
//   4. Atomically increments that LOD level's draw command instanceCount
//
// For visualization, EVERY instance is drawn (nothing is skipped). The frustum-test result
// is packed into the top bit of the stored instance index (INSTANCE_CULLED_FLAG): the vertex
// shader unpacks it and the fragment shader tints in-frustum instances green and culled ones red.
//
// The args buffer contains one DrawIndexedIndirectCommand per LOD level (across all groups).
// The instance index list buffer holds compacted instance indices per LOD level.
// The count buffer holds the total number of non-zero draws (for DrawIndexedIndirectCount).
//
// Binding layout (set 0):
//   0: InstanceBuffer (SSBO, read)
//   1: LodLevelBuffer (SSBO, read)
//   2: LodGroupBuffer (SSBO, read)
//   3: ArgsBuffer (RWSSBO, write)
//   4: CountBuffer (RWSSBO, write)
//   5: CameraBuffer (UBO, read)
//   6: InstanceIndexListBuffer (RWSSBO, write) — compacted instance indices per LOD level

struct InstanceData
{
    float4x4 modelMatrix;  // Row-major (CPU transposes before upload)
    uint     groupIndex;
    uint     _pad[3];
};

struct LodLevelGpu
{
    uint firstVertex;
    uint vertexCount;
    uint firstIndex;
    uint indexCount;
    float screenRatio;
    uint instanceListOffset;  // Offset into the global instance index list
    uint _pad[2];
};

struct LodGroupGpu
{
    uint levelOffset;
    uint levelCount;
    uint _pad[2];
};

struct CameraData
{
    float4x4 viewMatrix;
    float4x4 projMatrix;
    float4   cameraPos;     // xyz = position
    float4   screenParams;  // x = width, y = height, z = fov, w = aspect
    uint     instanceCount;
    uint     groupCount;
    uint     _pad[2];
};

struct DrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

[[vk::binding(0, 0)]] StructuredBuffer<InstanceData>  instances;
[[vk::binding(1, 0)]] StructuredBuffer<LodLevelGpu>   lodLevels;
[[vk::binding(2, 0)]] StructuredBuffer<LodGroupGpu>   lodGroups;
[[vk::binding(3, 0)]] RWStructuredBuffer<DrawIndexedIndirectCommand> drawArgs;
[[vk::binding(4, 0)]] RWStructuredBuffer<uint>        drawCount;
[[vk::binding(5, 0)]] ConstantBuffer<CameraData>      camera;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint>        instanceIndexList;

// Top bit of a stored instance index marks the instance as frustum-culled.
// Instance counts are far below 2^31, so the remaining 31 bits hold the real index.
static const uint INSTANCE_CULLED_FLAG = 0x80000000u;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 globalID : SV_DispatchThreadID)
{
    uint instanceIdx = globalID.x;
    if (instanceIdx >= camera.instanceCount) return;

    // Args and count buffers are zeroed on the CPU before dispatch.
    // InterlockedAdd on instanceCount will return 0 for the first instance of each
    // LOD level, triggering the draw command setup below.

    InstanceData inst = instances[instanceIdx];
    uint groupIdx = inst.groupIndex;
    if (groupIdx >= camera.groupCount) return;

    LodGroupGpu group = lodGroups[groupIdx];
    if (group.levelCount == 0) return;

    // Extract instance world position from model matrix (row-major: last column of each row is translation)
    float3 instancePos = float3(inst.modelMatrix[0].w, inst.modelMatrix[1].w, inst.modelMatrix[2].w);

    // Estimate bounding sphere radius (unit-scale shapes)
    float boundingRadius = 1.0f;

    // Frustum test. The result only decides the debug color; the instance is drawn either way.
    float4 planes[6];
    float4x4 viewProj = mul(camera.projMatrix, camera.viewMatrix);
    ExtractFrustumPlanes(viewProj, planes);

    bool visible = IsSphereInFrustum(planes, instancePos, boundingRadius);

    // Compute screen-space ratio
    float3 toCamera = float3(camera.cameraPos.x, camera.cameraPos.y, camera.cameraPos.z) - instancePos;
    float distance = length(toCamera);
    float tanHalfFov = tan(camera.screenParams.z * 0.5);
    float screenRatio = boundingRadius / max(distance * tanHalfFov, 0.001);

    // Select LOD level: find the first level whose screenRatio threshold is <= our screenRatio
    uint selectedLevel = group.levelCount - 1; // Default to lowest quality
    for (uint l = 0; l < group.levelCount; l++)
    {
        LodLevelGpu level = lodLevels[group.levelOffset + l];
        if (screenRatio >= level.screenRatio)
        {
            selectedLevel = l;
            break;
        }
    }

    uint globalLodIdx = group.levelOffset + selectedLevel;
    LodLevelGpu level = lodLevels[globalLodIdx];

    // Atomically get a slot in this LOD level's instance index list
    uint slotIndex;
    InterlockedAdd(drawArgs[globalLodIdx].instanceCount, 1, slotIndex);

    // Write the instance index into the compacted list, tagging culled instances in the top bit
    uint packedIdx = visible ? instanceIdx : (instanceIdx | INSTANCE_CULLED_FLAG);
    instanceIndexList[level.instanceListOffset + slotIndex] = packedIdx;

    // First instance for this LOD level — set up the rest of the draw command
    if (slotIndex == 0)
    {
        drawArgs[globalLodIdx].indexCount = level.indexCount;
        drawArgs[globalLodIdx].firstIndex = level.firstIndex;
        drawArgs[globalLodIdx].vertexOffset = int(level.firstVertex);
        drawArgs[globalLodIdx].firstInstance = level.instanceListOffset; // Offset into index list

        // Increment the draw count (for DrawIndexedIndirectCount)
        uint oldCount;
        InterlockedAdd(drawCount[0], 1, oldCount);
    }
}
