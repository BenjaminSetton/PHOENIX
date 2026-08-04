#pragma once

#include <glm.hpp>
#include <vector>

#include "../../common/src/base_sample.h"

// Grid control-point vertex
struct GridVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
};

// Camera uniform buffer (matrices transposed for row-major Slang convention)
struct CameraUBO
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 camPos;
	float    _pad;
};

// Tessellation LOD parameters
struct TessParams
{
	float maxTessFactor;     // Tessellation factor at the closest distance
	float minDistance;       // Distance at which max tessellation kicks in
	float maxDistance;       // Distance at which tessellation drops to 1
	float displacementScale; // Heightmap displacement amplitude
};

class TessellationSample : public Common::BaseSample
{
public:

	TessellationSample();
	~TessellationSample() override;

	void Draw() override;

protected:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

private:

	void CreateUniformCollection();
	void BuildImGuiUI();
	void RegenerateGrid();

private:

	// Shaders
	std::vector<PHX::ShaderHandle> m_shaders;

	// Uniforms
	PHX::UniformCollectionHandle m_uniformCollection;

	// Buffers
	PHX::BufferHandle m_vertexBuffer;
	PHX::BufferHandle m_cameraBuffer;
	PHX::BufferHandle m_tessParamsBuffer;

	// Depth buffer
	PHX::TextureHandle m_depthBuffer;

	// Pipeline descriptions
	PHX::GraphicsPipelineDesc m_solidPipelineDesc;
	PHX::GraphicsPipelineDesc m_wireframePipelineDesc;
	std::vector<PHX::InputAttribute> m_inputAttributes;

	// Camera data
	CameraUBO m_cameraData;

	// Grid parameters
	uint32_t m_gridWidth;        // Number of quads along X
	uint32_t m_gridHeight;       // Number of quads along Z
	float    m_cellSize;         // World-space size of each quad cell
	uint32_t m_patchCount;       // Total number of patches (quads)
	uint32_t m_vertexCount;      // Total control-point vertices
	std::vector<GridVertex> m_gridVertices; // CPU-side grid data for upload
	bool m_gridDirty;            // True when grid params changed and vertices need re-upload

	// ImGui-tunable parameters
	float m_maxTessFactor;
	float m_minDistance;
	float m_maxDistance;
	float m_displacementScale;
	bool  m_wireframe;

	// Stats
	float m_lastFrameTime;
};
