#pragma once

#include "../../common/src/base_sample.h"

struct ParticleData
{
	glm::mat4 transform;
	glm::vec4 color;
};

struct SimData
{
	float dt = 0.0f;
	float totalTime = 0.0f;
	uint32_t totalParticles = 0;
};

struct CameraData
{
	glm::mat4 view;
	glm::mat4 proj;
};

class ComputeParticlesSample : public Common::BaseSample
{
public:

	ComputeParticlesSample();
	~ComputeParticlesSample() override;

	bool Update(float dt) override;
	void Draw() override;

private:

	void Init() override;
	void Shutdown() override;

	void CreateUniformCollection();
	void CreateDrawUniformCollection();
	void CreateOutlineUniformCollection();

	void InitializeParticleBuffer();
	void InitializeOutlineBuffers();

private:

	std::vector<PHX::ShaderHandle> m_shaders;
	std::vector<PHX::ShaderHandle> m_drawShaders;

	PHX::UniformCollectionHandle m_uniformCollection;
	PHX::UniformCollectionHandle m_drawUniformCollection;

	PHX::BufferHandle m_particlesBuffer;
	PHX::BufferHandle m_simDataBuffer;
	PHX::BufferHandle m_cameraBuffer;

	PHX::TextureHandle m_depthBuffer;

	PHX::ComputePipelineDesc m_particlesPipelineDesc;
	PHX::GraphicsPipelineDesc m_drawPipelineDesc;

	std::vector<PHX::ShaderHandle> m_outlineShaders;
	PHX::UniformCollectionHandle m_outlineUniformCollection;
	PHX::BufferHandle m_outlineVertexBuffer;
	PHX::BufferHandle m_outlineIndexBuffer;
	PHX::GraphicsPipelineDesc m_outlinePipelineDesc;
	PHX::InputAttribute m_outlineInputAttribute;

	SimData m_simData;
	CameraData m_cameraData;

	int32_t m_volumeMinBound;
	int32_t m_volumeMaxBound;
};