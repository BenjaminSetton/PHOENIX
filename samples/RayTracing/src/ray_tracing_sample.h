#pragma once

#include <vector>

#include <glm.hpp>

#include "../../common/src/base_sample.h"

struct BlitVertex
{
	glm::vec2 pos;
	glm::vec2 uv;
};

class RayTracingSample : public Common::BaseSample
{
public:

	RayTracingSample();
	~RayTracingSample() override;

	bool Update(float dt) override;
	void Draw() override;

private:

	void Init() override;
	void Shutdown() override;

	void OverrideSettings(PHX::Settings& settings) override;

	void CreateUniformCollections();
	void UploadBlitVertices();

private:

	bool m_rayTracingSupported = false;

	PHX::UniformCollectionHandle m_rayTracingUniformCollection;
	PHX::UniformCollectionHandle m_blitUniformCollection;

	PHX::BufferHandle m_blitVertexBuffer;
	PHX::TextureHandle m_rayTracingOutput;

	PHX::RayTracingPipelineDesc m_rayTracingPipelineDesc;
	std::vector<PHX::ShaderHandle> m_rayTracingPipelineShaders;

	PHX::GraphicsPipelineDesc m_blitPipelineDesc;
	std::vector<PHX::ShaderHandle> m_blitPipelineShaders;

	std::vector<PHX::InputAttribute> m_blitInputAttributes;
};
