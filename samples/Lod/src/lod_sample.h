#pragma once

#include <PHX/phx.h>
#include <glm.hpp>

#include <vector>
#include <array>

#include "../../common/src/base_sample.h"
#include "../../common/src/camera/freefly_camera.h"
#include "../../common/src/lod_manager.h"
#include "asset_loader.h"

class LodSample : public Common::BaseSample
{
public:
	LodSample();
	~LodSample() override;

	void Draw() override;

protected:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

private:

	// Shared projection parameters. The LOD/frustum camera, the observer camera and
	// the frustum visualization all use the SAME projection so the green frustum viz
	// exactly represents the volume that culling is performed against.
	static constexpr float kFovDegrees = 60.0f;
	static constexpr float kNearPlane  = 0.5f;
	static constexpr float kFarPlane   = 400.0f;

	void CreateUniformCollections();
	void GenerateInstances();
	void ComputeFrustumVertices();

	// Builds the projection matrix used for culling, rendering and the frustum viz
	// (includes the Vulkan Y-flip). Depth range is GLM default [-1, 1].
	glm::mat4 GetProjectionMatrix() const;

	struct DebugLineVertex { glm::vec3 pos; glm::vec3 color; };

	// LOD manager
	Common::LodManager m_lodManager;

	// Cameras
	// - m_pLodCamera is the "player": culling and the frustum viz are always computed
	//   from this camera, whether or not it is the one receiving input.
	// - m_pFreeflyCamera is the observer used to inspect the frozen frustum + culling.
	Common::FreeflyCamera* m_pLodCamera = nullptr;
	Common::FreeflyCamera* m_pFreeflyCamera = nullptr;
	bool m_freeflyActive = false;  // Which camera receives input

	// Shaders
	std::vector<PHX::ShaderHandle> m_computeShaders;
	std::vector<PHX::ShaderHandle> m_meshShaders;
	std::vector<PHX::ShaderHandle> m_debugLineShaders;

	// Pipelines
	PHX::ComputePipelineDesc  m_computePipelineDesc;
	PHX::GraphicsPipelineDesc m_meshPipelineDesc;
	PHX::GraphicsPipelineDesc m_debugLinePipelineDesc;

	// Uniform collections
	PHX::UniformCollectionHandle m_computeUniforms;
	PHX::UniformCollectionHandle m_meshUniforms;
	PHX::UniformCollectionHandle m_debugLineUniforms;

	// Input attributes
	std::vector<PHX::InputAttribute> m_meshInputAttributes;
	std::vector<PHX::InputAttribute> m_debugLineInputAttributes;

	// Depth buffer
	PHX::TextureHandle m_depthBuffer;

	// Debug line buffers (frustum visualization)
	PHX::BufferHandle m_debugLineVertexBuffer;
	PHX::BufferHandle m_debugCameraBuffer;  // UBO for debug line shader (active camera)
	std::vector<DebugLineVertex> m_frustumVertices; // CPU-side frustum edges, rebuilt each frame
	uint32_t m_debugLineVertexCount = 0;

	// Instance data
	std::vector<Common::LodInstanceData> m_instances;
	uint32_t m_instanceCount = 1000;
	bool m_instancesDirty = true;

	// Settings (ImGui-controlled)
	bool m_wireframe = false;
	bool m_showFrustum = true;
	float m_lodDistanceScale = 1.0f;

	// Capability
	bool m_useIndirectCount = false;

	// Camera UBO data for debug lines
	struct DebugCameraData
	{
		glm::mat4 viewMatrix;
		glm::mat4 projMatrix;
	};
	DebugCameraData m_debugCameraData;
};
