#pragma once

#include <glm.hpp>
#include <vector>

#include "../../common/src/base_sample.h"
#include "asset_loader.h"

struct TransformData
{
	glm::mat4 worldMat;
	glm::mat4 viewMat;
	glm::mat4 projMat;
};

class BasicModelSample : public Common::BaseSample
{
public:

	BasicModelSample();
	~BasicModelSample() override;

	bool Update(float dt) override;
	void Draw() override;

private:

	void Init() override;
	void Shutdown() override;

	void CreateUniformCollection();
	void UploadMeshDataToGPU();

private:

	TransformData m_transform;

	std::vector<PHX::ShaderHandle> m_shaders;
	std::vector<PHX::InputAttribute> m_inputAttributes;
	PHX::GraphicsPipelineDesc m_pipelineDesc;

	PHX::TextureHandle m_depthBuffer;
	PHX::UniformCollectionHandle m_uniformCollection;
	PHX::BufferHandle m_uniformBuffer;
	PHX::BufferHandle m_vertexBuffer;
	PHX::BufferHandle m_indexBuffer;

	Common::AssetHandle m_assetID;
};
