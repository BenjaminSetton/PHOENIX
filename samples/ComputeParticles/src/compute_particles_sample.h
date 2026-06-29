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
	void SeedParticles();

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

	SimData m_simData;
	CameraData m_cameraData;
};