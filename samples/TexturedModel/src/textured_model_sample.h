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

	PHX::TextureHandle m_depthBuffer;
	PHX::UniformCollectionHandle m_uniformCollection;
	PHX::BufferHandle m_transformBuffer;
	PHX::BufferHandle m_cameraBuffer;
	PHX::BufferHandle m_vertexBuffer;
	PHX::BufferHandle m_indexBuffer;

	std::vector<PHX::ShaderHandle> m_shaders;

	std::vector<PHX::InputAttribute> m_inputAttributes;

	std::vector<PHX::TextureHandle> m_assetTextures;

	Common::AssetHandle m_assetID;

};