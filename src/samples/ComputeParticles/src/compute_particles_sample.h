#pragma once

#include <random>

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
	float shouldExplode = 0.0f;
	u32 totalParticles = 0;
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

	void Draw() override;

private:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

	void CreateComputeUniformCollection();
	void CreateDrawUniformCollection();
	void CreateOutlineUniformCollection();

	void InitializeParticleBuffer();
	void InitializeOutlineBuffers();

private:

	std::vector<PHX::ShaderHandle> m_particleComputeShader;
	std::vector<PHX::ShaderHandle> m_drawShaders;

	PHX::UniformCollectionHandle m_computeUniformCollection;
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

	i32 m_volumeMinBound;
	i32 m_volumeMaxBound;

	std::random_device m_randomEngine;
	std::mt19937 m_mt;
};