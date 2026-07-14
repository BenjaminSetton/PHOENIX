#pragma once

#include <string>
#include <vector>

#include <glm.hpp>

#include "../../common/src/base_sample.h"
#include "../../common/src/camera/freefly_camera.h"
#include "asset_loader.h"

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

	void LoadSceneAssets();
	void CreateSceneTextures();
	void CreateSceneGeometryBuffers();
	void BuildSceneAccelerationStructures();
	void UpdateCameraData(float dt);

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

	Common::AssetHandle m_assetHandle = Common::INVALID_ASSET_HANDLE;
	const AssetType* m_pAsset = nullptr;

	PHX::BufferHandle m_sceneVertexBuffer;
	PHX::BufferHandle m_sceneIndexBuffer;
	PHX::BufferHandle m_instanceBuffer;
	PHX::BufferHandle m_cameraUniformBuffer;
	PHX::BufferHandle m_geometryInfoBuffer;

	std::vector<PHX::TextureHandle> m_sceneTextures;

	PHX::AccelerationStructureHandle m_tlas;
	std::vector<PHX::AccelerationStructureHandle> m_blas;
	std::vector<std::string> m_blasNames;

	glm::mat4 m_projMatrix = glm::mat4(1.0f);

	struct CameraData
	{
		glm::mat4 viewInverse;
		glm::mat4 projInverse;
		glm::vec3 cameraPosition;
		float padding0;
		glm::vec2 viewport;
		glm::vec2 padding1;
	};
	CameraData m_cameraData{};

};
