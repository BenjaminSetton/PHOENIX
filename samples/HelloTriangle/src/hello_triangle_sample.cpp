
#include <array>

#include "hello_triangle_sample.h"

#include "../../common/src/utils/shader_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static constexpr u32 VERTEX_COUNT = 3;
static constexpr SimpleVertexType triVerts[VERTEX_COUNT] =
{
	{{ -0.5f, 0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }},
	{{ 0.0f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }},
	{{ 0.5f, 0.5f, 0.0f, 1.0f } , { 0.0f, 0.0f, 1.0f, 1.0f }}
};

HelloTriangleSample::HelloTriangleSample() : m_testUBO()
{
	Init();
}

HelloTriangleSample::~HelloTriangleSample()
{
	Shutdown();
}

bool HelloTriangleSample::Update(float dt)
{
	bool shouldClose = BaseSample::Update(dt);

	m_testUBO.time += dt;

	return shouldClose;
}

void HelloTriangleSample::Draw()
{
	ClearValues clearColor{};
	clearColor.color.color = { 0.1f, 0.1f, 0.1f, 0.0f };
	clearColor.useClearColor = true;

	m_renderGraph.BeginFrame(m_swapChain);

	RenderPassHandle renderPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("HelloTriangle", BIND_POINT::GRAPHICS, renderPass);
	CHECK_PHX_RES(phxRes);

	renderPass.SetBufferInput(m_vertexBuffer);
	renderPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
	renderPass.SetPipelineDescription(m_pipelineDesc);
	renderPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(m_uniformBuffer, &m_testUBO, sizeof(TestUBO));
		m_uniformCollection.QueueBufferUpdate(m_uniformBuffer, 0, 0, 0);
		m_uniformCollection.FlushUpdateQueue();

		deviceContext.BindUniformCollection(m_uniformCollection);
		deviceContext.BindVertexBuffer(m_vertexBuffer);
		deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.Draw(VERTEX_COUNT);
	});

	m_renderGraph.Bake(m_swapChain);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./HelloTriangle_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame();

	m_swapChain.Present();
}

void HelloTriangleSample::Init()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | HELLO TRIANGLE", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// SHADERS
	ShaderHandle vertShader;
	if (!Common::AllocateShader("../src/shaders/vertex_sample.vert", SHADER_STAGE::VERTEX, m_renderDevice, vertShader))
	{
		return;
	}

	ShaderHandle fragShader;
	if (!Common::AllocateShader("../src/shaders/fragment_sample.frag", SHADER_STAGE::FRAGMENT, m_renderDevice, fragShader))
	{
		return;
	}

	m_shaders.push_back(vertShader);
	m_shaders.push_back(fragShader);

	// VERTEX BUFFER
	BufferCreateInfo vBufferCI{};
	vBufferCI.pName = "VertexBuffer";
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = sizeof(SimpleVertexType) * VERTEX_COUNT;
	phxRes = m_renderDevice.AllocateBuffer(vBufferCI, m_vertexBuffer);
	CHECK_PHX_RES(phxRes);

	// UNIFORM BUFFER
	m_testUBO.time = 0.0f;

	BufferCreateInfo uniformBufferCI{};
	uniformBufferCI.pName = "UniformBuffer";
	uniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	uniformBufferCI.sizeBytes = sizeof(TestUBO);
	phxRes = m_renderDevice.AllocateBuffer(uniformBufferCI, m_uniformBuffer);
	CHECK_PHX_RES(phxRes);

	// UNIFORM COLLECTION
	CreateUniformCollection();

	// INPUT ATTRIBUTES
	m_inputAttributes =
	{
		// POSITION
		{
			0,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32A32_FLOAT	// format
		},
		// COLOR
		{
			1,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32A32_FLOAT	// format
		},
	};

	// GRAPHICS PIPELINE
	m_pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_pipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_pipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_pipelineDesc.viewportSize = { m_window.GetCurrentWidth(), m_window.GetCurrentHeight() };
	m_pipelineDesc.pShaders = m_shaders.data();
	m_pipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
	m_pipelineDesc.cullMode = CULL_MODE::NONE;
	m_pipelineDesc.uniformCollection = m_uniformCollection;

	// Upload mesh to GPU
	UploadMeshDataToGPU();
}

void HelloTriangleSample::Shutdown()
{
	m_shaders.clear();
}

void HelloTriangleSample::CreateUniformCollection()
{
	UniformData uniform{};
	uniform.binding = 0;
	uniform.shaderStage = SHADER_STAGE_FLAG_FRAGMENT;
	uniform.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup uniformDataGroup{};
	uniformDataGroup.set = 0;
	uniformDataGroup.uniformArray = &uniform;
	uniformDataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = &uniformDataGroup;
	uniformCollectionCI.groupCount = 1;

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_uniformCollection);
	CHECK_PHX_RES(phxRes);
}

void HelloTriangleSample::UploadMeshDataToGPU()
{
	RenderPassHandle renderPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("MeshDataUpload", BIND_POINT::TRANSFER, renderPass);
	CHECK_PHX_RES(phxRes);

	renderPass.SetBufferOutput(m_vertexBuffer);
	renderPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(m_vertexBuffer, &triVerts, sizeof(SimpleVertexType) * VERTEX_COUNT);
	});
}
