struct BoneInfo
{
    int parentIndex;
    uint nodeIndex;
    float2 _pad;
    float4x4 offsetMatrix;
    float4x4 bindLocalTransform;
};

struct Keyframe
{
    float time;
    float _pad0[3];
    float4 translation;
    float4 rotation;
    float4 scale;
};

struct ChannelInfo
{
    uint nodeIndex;
    uint keyframeOffset;
    uint keyframeCount;
    uint _pad;
};

struct ClipInfo
{
    float duration;
    float ticksPerSecond;
    uint channelOffset;
    uint channelCount;
};

struct InstanceData
{
    float4x4 modelMatrix;
    float timeOffset;
    uint clipIndex;
    uint _pad[2];
};

struct AnimParams
{
    float globalTime;
    uint boneCount;
    uint instanceCount;
    float _pad;
};

[[vk::binding(0, 0)]] StructuredBuffer<BoneInfo> bones;
[[vk::binding(1, 0)]] StructuredBuffer<Keyframe> keyframes;
[[vk::binding(2, 0)]] StructuredBuffer<ClipInfo> clips;
[[vk::binding(3, 0)]] StructuredBuffer<ChannelInfo> channels;
[[vk::binding(4, 0)]] StructuredBuffer<InstanceData> instances;
[[vk::binding(5, 0)]] RWStructuredBuffer<float4x4> boneMatrices;
[[vk::binding(6, 0)]] ConstantBuffer<AnimParams> animParams;
[[vk::binding(7, 0)]] StructuredBuffer<int> nodeParents;
[[vk::binding(8, 0)]] StructuredBuffer<float4x4> nodeLocalTransforms;

// Build a transformation matrix from translation, rotation (quaternion), and scale.
// All matrices use Slang's default row-major layout (CPU transposes before upload).
float4x4 BuildMatrix(float4 translation, float4 rotation, float4 scale)
{
    float qx = rotation.x;
    float qy = rotation.y;
    float qz = rotation.z;
    float qw = rotation.w;

    float n = qx * qx + qy * qy + qz * qz + qw * qw;
    if (n > 0.0)
    {
        float s = 2.0 / n;
        float xs = qx * s, ys = qy * s, zs = qz * s;
        float wx = qw * xs, wy = qw * ys, wz = qw * zs;
        float xx = qx * xs, xy = qx * ys, xz = qx * zs;
        float yy = qy * ys, yz = qy * zs, zz = qz * zs;

        // Quaternion-to-rotation-matrix, matching GLM's mat3_cast(quat).
        // GLM stores column-major; we construct with GLSL columns as Slang rows
        // then transpose to get the correct row-major rotation matrix.
        float3x3 rot = float3x3(
            1.0 - (yy + zz), xy + wz,          xz - wy,
            xy - wz,          1.0 - (xx + zz), yz + wx,
            xz + wy,          yz - wx,          1.0 - (xx + yy)
        );
        rot = transpose(rot);

        // TRS matrix in row-major form.
        float4x4 m = float4x4(
            scale.x * rot[0][0], scale.y * rot[0][1], scale.z * rot[0][2], translation.x,
            scale.x * rot[1][0], scale.y * rot[1][1], scale.z * rot[1][2], translation.y,
            scale.x * rot[2][0], scale.y * rot[2][1], scale.z * rot[2][2], translation.z,
            0.0,                 0.0,                 0.0,                 1.0
        );
        return m;
    }
    return float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

// Spherical linear interpolation between two quaternions
float4 Slerp(float4 q0, float4 q1, float t)
{
    float dotProduct = dot(q0, q1);
    if (dotProduct < 0.0)
    {
        q1 = -q1;
        dotProduct = -dotProduct;
    }
    if (dotProduct > 0.9995)
    {
        return normalize(lerp(q0, q1, t));
    }
    float theta = acos(dotProduct);
    float sinTheta = sin(theta);
    float w0 = sin((1.0 - t) * theta) / sinTheta;
    float w1 = sin(t * theta) / sinTheta;
    return w0 * q0 + w1 * q1;
}

// Find the animation channel for a given node index and compute its interpolated local transform.
// Returns true if a channel was found, false otherwise (caller should use bind-pose transform).
bool ComputeChannelTransform(uint nodeIndex, ClipInfo clip, float animTime, out float4x4 result)
{
    for (uint c = clip.channelOffset; c < clip.channelOffset + clip.channelCount; c++)
    {
        ChannelInfo chInfo = channels[c];
        if (chInfo.nodeIndex != nodeIndex)
            continue;

        if (chInfo.keyframeCount == 0)
        {
            result = float4x4(
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0
            );
            return true;
        }
        if (chInfo.keyframeCount == 1)
        {
            Keyframe kf = keyframes[chInfo.keyframeOffset];
            result = BuildMatrix(kf.translation, kf.rotation, kf.scale);
            return true;
        }

        uint kf0 = 0;
        uint kf1 = 1;
        for (uint k = 0; k < chInfo.keyframeCount - 1; k++)
        {
            if (keyframes[chInfo.keyframeOffset + k].time <= animTime &&
                keyframes[chInfo.keyframeOffset + k + 1].time >= animTime)
            {
                kf0 = k;
                kf1 = k + 1;
                break;
            }
        }

        if (animTime >= keyframes[chInfo.keyframeOffset + chInfo.keyframeCount - 1].time)
        {
            kf0 = chInfo.keyframeCount - 1;
            kf1 = kf0;
        }
        if (animTime <= keyframes[chInfo.keyframeOffset].time)
        {
            kf0 = 0;
            kf1 = 0;
        }

        Keyframe k0 = keyframes[chInfo.keyframeOffset + kf0];
        Keyframe k1 = keyframes[chInfo.keyframeOffset + kf1];

        float t = 0.0;
        if (k1.time > k0.time)
        {
            t = clamp((animTime - k0.time) / (k1.time - k0.time), 0.0, 1.0);
        }

        float4 trans = lerp(k0.translation, k1.translation, t);
        float4 rot = Slerp(k0.rotation, k1.rotation, t);
        float4 scl = lerp(k0.scale, k1.scale, t);

        result = BuildMatrix(trans, rot, scl);
        return true;
    }
    result = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    return false;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 globalID : SV_DispatchThreadID)
{
    if (animParams.boneCount == 0) return;

    uint instanceIdx = globalID.x / animParams.boneCount;
    uint boneIdx = globalID.x % animParams.boneCount;

    if (instanceIdx >= animParams.instanceCount) return;

    InstanceData inst = instances[instanceIdx];
    uint clipIdx = inst.clipIndex;

    if (clipIdx >= 16) return;

    ClipInfo clip = clips[clipIdx];
    float animTime = fmod(animParams.globalTime + inst.timeOffset, clip.duration / max(clip.ticksPerSecond, 0.001)) * max(clip.ticksPerSecond, 0.001);

    uint nodeIdx = bones[boneIdx].nodeIndex;

    // Build the node chain from root to this bone's node (max 64 levels deep)
    int chain[64];
    int chainLen = 0;
    int currentNode = int(nodeIdx);
    while (currentNode >= 0 && chainLen < 64)
    {
        chain[chainLen] = currentNode;
        currentNode = nodeParents[currentNode];
        chainLen++;
    }

    // Compose global transform from root to leaf (row-major mul)
    float4x4 globalTransform = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
    for (int i = chainLen - 1; i >= 0; i--)
    {
        int nIdx = chain[i];

        float4x4 localTransform;
        bool found = ComputeChannelTransform(uint(nIdx), clip, animTime, localTransform);
        if (!found)
        {
            localTransform = nodeLocalTransforms[nIdx];
        }

        globalTransform = mul(globalTransform, localTransform);
    }

    // Final bone matrix = globalTransform * offsetMatrix (row-major mul)
    float4x4 finalMatrix = mul(globalTransform, bones[boneIdx].offsetMatrix);

    boneMatrices[instanceIdx * animParams.boneCount + boneIdx] = finalMatrix;
}
