#pragma once

#include <glm.hpp>
#include <vector>

#include "../../common/src/asset_manager.h"
#include "../../common/src/base_sample.h"

struct TransformData
{
	glm::mat4 worldMat;
	glm::mat4 viewMat;
	glm::mat4 projMat;
};

class TexturedModelSample : public Common::BaseSample
{
public:

	TexturedModelSample();
	~TexturedModelSample() override;

	bool Update(float dt) override;
	void Draw() override;

protected:

	void Init() override;
	void Shutdown() override;

private:

	TransformData m_transform;

	PHX::GraphicsPipelineDesc m_pipelineDesc;

	PHX::ITexture* m_pDepthBuffer;
	PHX::IUniformCollection* m_pUniformCollection;
	PHX::IBuffer* m_pUniformBuffer;
	PHX::IBuffer* m_pVertexBuffer;
	PHX::IBuffer* m_pIndexBuffer;

	std::vector<PHX::IShader*> m_shaders;

	std::vector<PHX::InputAttribute> m_inputAttributes;

	Common::AssetHandle m_axeAssetID;

};