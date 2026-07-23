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

	void Draw() override;

private:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

	void OverrideSettings(PHX::Settings& settings) override;

	void CreateUniformCollections();
	void UploadBlitVertices();

	void LoadSceneAssets();
	void CreateDefaultTextures();
	void CreateSceneTextures();
	void CreateSceneGeometryBuffers();
	void BuildSceneAccelerationStructures();
	void UpdateCameraData(float dt);
	void LoadEnvironmentMap();
	void CreateAccumulationImages();

private:

	bool m_rayTracingSupported = false;

	PHX::UniformCollectionHandle m_rayTracingUniformCollection;
	PHX::UniformCollectionHandle m_blitUniformCollection;

	PHX::BufferHandle m_blitVertexBuffer;
	PHX::TextureHandle m_rayTracingOutput;

	PHX::RayTracingPipelineDesc m_rayTracingPipelineDesc;
	std::vector<PHX::ShaderHandle> m_rayTracingPipelineShaders;
	std::vector<PHX::HitGroupDesc> m_rayTracingHitGroups;

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
	PHX::BufferHandle m_materialBuffer;

	// Environment map
	PHX::TextureHandle m_equirectTexture;
	PHX::TextureHandle m_envCubeMap;
	PHX::BufferHandle m_cubeFaceSizeBuffer;

	// Equirect-to-cube compute pipeline
	PHX::ComputePipelineDesc m_equirectToCubePipelineDesc;
	std::vector<PHX::ShaderHandle> m_equirectToCubeShaders;
	PHX::UniformCollectionHandle m_equirectToCubeUniformCollection;

	// Progressive accumulation
	PHX::TextureHandle m_accumulationImageA;
	PHX::u32 m_frameCount = 0;
	bool m_resetAccumulation = true;
	glm::vec3 m_prevCameraPosition = glm::vec3(0.0f);
	glm::vec3 m_prevCameraForward = glm::vec3(0.0f);

	std::vector<PHX::TextureHandle> m_sceneTextures;
	// Maps an index into m_pAsset->textures to its actual slot in m_sceneTextures.
	// Needed because CreateSceneTextures may skip textures (e.g. 0 mip levels), which
	// would otherwise desync a naive "textureIndex + defaultTextureCount" offset.
	std::vector<PHX::u32> m_textureIndexRemap;

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
		PHX::u32 frameCount;
		PHX::u32 resetAccumulation;
		glm::vec2 padding1;
	};
	CameraData m_cameraData{};

};
