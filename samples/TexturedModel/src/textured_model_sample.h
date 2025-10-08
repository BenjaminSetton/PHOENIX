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

struct CameraData
{
	glm::vec3 cameraPos;
};

enum class PBRTextureType
{

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

	// Create PHX::ITexture objects for m_assetID;
	void CreateAssetTextures();

	// Creates uniform collection for all shader resources. Stored result in m_pUniformCollection
	void CreateUniformCollection();

	void UploadMeshDataToGPU();

private:

	TransformData m_transform;

	PHX::GraphicsPipelineDesc m_pipelineDesc;

	PHX::ITexture* m_pDepthBuffer;
	PHX::IUniformCollection* m_pUniformCollection;
	PHX::IBuffer* m_pTransformBuffer;
	PHX::IBuffer* m_pCameraBuffer;
	PHX::IBuffer* m_pVertexBuffer;
	PHX::IBuffer* m_pIndexBuffer;

	std::vector<PHX::IShader*> m_shaders;

	std::vector<PHX::InputAttribute> m_inputAttributes;

	std::vector<PHX::ITexture*> m_assetTextures;

	Common::AssetHandle m_assetID;

};