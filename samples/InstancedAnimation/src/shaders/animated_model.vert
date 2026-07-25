#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;

struct InstanceData
{
    mat4 modelMatrix;
    float timeOffset;
    uint clipIndex;
    uint _pad[2];
};

layout(set = 0, binding = 0) readonly buffer BoneMatrixBuffer
{
    mat4 boneMatrices[];
};

layout(set = 0, binding = 1) readonly buffer InstanceBuffer
{
    InstanceData instances[];
};

layout(set = 0, binding = 2) uniform CameraData
{
    mat4 view;
    mat4 proj;
    uint boneCount;
    uint _pad[3];
};

void main()
{
    uint instanceIdx = gl_InstanceIndex;

    // Skin the vertex
    mat4 skinMatrix = mat4(0.0);
    for (int i = 0; i < 4; i++)
    {
        uint boneIdx = inBoneIndices[i];
        float weight = inBoneWeights[i];
        if (weight > 0.0)
        {
            skinMatrix += boneMatrices[instanceIdx * boneCount + boneIdx] * weight;
        }
    }

    vec4 skinnedPos = skinMatrix * vec4(inPosition, 1.0);
    vec4 skinnedNormal = skinMatrix * vec4(inNormal, 0.0);

    InstanceData inst = instances[instanceIdx];
    vec4 worldPos = inst.modelMatrix * skinnedPos;

    gl_Position = proj * view * worldPos;

    fragNormal = normalize(mat3(inst.modelMatrix * skinMatrix) * inNormal);
    fragUV = inUV;
}
