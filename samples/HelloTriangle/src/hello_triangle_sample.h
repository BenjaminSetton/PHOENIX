#pragma once

#include <vector>

#include "../../common/src/base_sample.h"

struct SimpleVertexType
{
	float pos[4];
	float color[4];
};

struct TestUBO
{
	float time;
};

class HelloTriangleSample : public Common::BaseSample
{
public:

	HelloTriangleSample();
	~HelloTriangleSample() override;

	void Draw() override;

private:

	void InitSample() override;
	void ShutdownSample() override;
	void UpdateSample(float dt) override;

	void CreateUniformCollection();
	void UploadMeshDataToGPU();

private:

	std::vector<PHX::ShaderHandle> m_shaders;

	PHX::UniformCollectionHandle m_uniformCollection;
	PHX::BufferHandle m_vertexBuffer;
	PHX::BufferHandle m_uniformBuffer;

	std::vector<PHX::InputAttribute> m_inputAttributes;
	PHX::GraphicsPipelineDesc m_pipelineDesc;

	TestUBO m_testUBO;
};
