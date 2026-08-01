#pragma once

#include <glm.hpp>
#include <vector>

#include "../../common/src/asset_manager.h"
#include "../../common/src/base_sample.h"
#include "asset_loader.h"

struct CameraData
{
	glm::mat4 view;
	glm::mat4 proj;
	uint32_t  boneCount;
	uint32_t  _padding[3];
};

struct AnimParamsData
{
	float globalTime;
	uint32_t boneCount;
	uint32_t instanceCount;
	float _padding;
};

struct InstanceData
{
	glm::mat4 modelMatrix;
	float     timeOffset;
	uint32_t  clipIndex;
	uint32_t  _padding[2];
};

// GPU-side structs for SSBOs (must match GLSL layout)
struct BoneInfoGPU
{
	int32_t  parentIndex;
	uint32_t nodeIndex;
	float    _padding[2];
	glm::mat4 offsetMatrix;
	glm::mat4 bindLocalTransform;  // bind-pose local transform of the corresponding node
};

struct KeyframeGPU
{
	float    time;
	float    _pad0[3];
	glm::vec4 translation;  // xyz used, w unused
	glm::vec4 rotation;     // quaternion (x, y, z, w)
	glm::vec4 scale;        // xyz used, w unused
};

struct ChannelInfoGPU
{
	uint32_t nodeIndex;
	uint32_t keyframeOffset;
	uint32_t keyframeCount;
	uint32_t _padding;
};

struct ClipInfoGPU
{
	float    duration;
	float    ticksPerSecond;
	uint32_t channelOffset;
	uint32_t channelCount;
};

enum class ClipAssignmentMode : int32_t
{
	ALL_SAME = 0,
	ROUND_ROBIN = 1,
	RANDOM = 2
};

class InstancedAnimationSample : public Common::BaseSample
{
public:

	InstancedAnimationSample();
	~InstancedAnimationSample() override;

	void Draw() override;

protected:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

private:

	void CreateAssetTextures();
	void CreateUniformCollections();
	void UploadDataToGPU();
	void RegenerateInstanceData();

	void BuildImGuiUI();

private:

	// Asset
	Common::AssetHandle m_assetID;

	// Shaders
	std::vector<PHX::ShaderHandle> m_computeShaders;
	std::vector<PHX::ShaderHandle> m_graphicsShaders;

	// Uniform collections
	PHX::UniformCollectionHandle m_computeUniformCollection;
	PHX::UniformCollectionHandle m_graphicsUniformCollection;

	// GPU buffers — static (uploaded once)
	PHX::BufferHandle m_vertexBuffer;
	PHX::BufferHandle m_indexBuffer;
	PHX::BufferHandle m_skeletonBuffer;      // BoneInfoGPU[]
	PHX::BufferHandle m_keyframeBuffer;      // KeyframeGPU[]
	PHX::BufferHandle m_channelInfoBuffer;   // ChannelInfoGPU[]
	PHX::BufferHandle m_clipInfoBuffer;      // ClipInfoGPU[]
	PHX::BufferHandle m_instanceBuffer;      // InstanceData[]
	PHX::BufferHandle m_nodeParentBuffer;    // int32_t[] — node parent indices
	PHX::BufferHandle m_nodeTransformBuffer; // mat4[] — node bind-pose local transforms

	// GPU buffers — dynamic (updated per frame)
	PHX::BufferHandle m_boneMatrixBuffer;    // mat4[instanceCount * boneCount]
	PHX::BufferHandle m_cameraBuffer;        // CameraData
	PHX::BufferHandle m_animParamsBuffer;    // AnimParamsData

	// Textures
	std::vector<PHX::TextureHandle> m_assetTextures;
	PHX::TextureHandle m_depthBuffer;

	// Pipeline descriptions
	PHX::ComputePipelineDesc m_computePipelineDesc;
	PHX::GraphicsPipelineDesc m_graphicsPipelineDesc;
	std::vector<PHX::InputAttribute> m_inputAttributes;

	// Instance data (CPU-side)
	std::vector<InstanceData> m_instances;
	bool m_instanceDataDirty;

	// Animation state
	float m_globalTime;
	bool  m_paused;
	float m_animSpeed;
	int   m_randomSeed;
	uint32_t m_activeClipIndex;
	ClipAssignmentMode m_clipMode;
	uint32_t m_instanceCount;

	// Camera data
	CameraData m_cameraData;

	// Stats for ImGui
	float m_lastFrameTime;
};
