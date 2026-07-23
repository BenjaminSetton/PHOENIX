
#include <array>
#include <gtc/matrix_transform.hpp>

#include "basic_model_sample.h"

#include "../../common/src/utils/shader_utils.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static TransformData InitializeTransform(glm::vec3 initialCameraPos, float FOV, float aspectRatio)
{
	TransformData data;

	// World
	data.worldMat = glm::identity<glm::mat4>();
	data.worldMat = glm::scale(data.worldMat, glm::vec3(0.025f));

	// View (toward -Z)
	glm::vec3 eye = initialCameraPos;
	glm::vec3 center = { 0.0f, 0.0f, -1.0f };
	glm::vec3 up = { 0.0f, 1.0f, 0.0f };
	data.viewMat = glm::inverse(glm::lookAt(eye, center, up));

	// Perspective
	data.projMat = glm::perspective(FOV, aspectRatio, 0.01f, 1000.0f);
	data.projMat[1][1] *= -1.0f;

	return data;
}

BasicModelSample::BasicModelSample() : m_transform(), m_pipelineDesc(),
	m_depthBuffer(), m_uniformCollection(), m_uniformBuffer(),
	m_vertexBuffer(), m_indexBuffer(), m_assetID(Common::INVALID_ASSET_HANDLE)
{
}

BasicModelSample::~BasicModelSample()
{
}

void BasicModelSample::UpdateSample(float dt)
{
	(void)dt;

	// Update the model's transform
	m_transform.worldMat = glm::rotate(m_transform.worldMat, 0.02f, { 0.0f, -1.0f, 0.0f });
	m_transform.worldMat = glm::rotate(m_transform.worldMat, 0.02f, { 1.0f, 0.0f, 0.0f });
}

void BasicModelSample::Draw()
{
	const AssetType* cubeAsset = AssetManager::Get().GetAsset(m_assetID);
	if (cubeAsset == nullptr)
	{
		return;
	}

	ClearValues clearColor{};
	clearColor.color.color = Vec4f(0.5f, 0.75f, 0.98f, 1.0f);
	clearColor.useClearColor = true;

	m_renderGraph.BeginFrame(m_swapChain);

	RenderPassHandle renderPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("BasicCubePass", PASS_TYPE::GRAPHICS, renderPass);
	CHECK_PHX_RES(phxRes);

	renderPass.SetBufferInput(m_vertexBuffer);
	renderPass.SetBufferInput(m_indexBuffer);
	renderPass.SetTextureOutput(m_swapChain.GetCurrentImage(), ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, clearColor);
	renderPass.SetDepthOutput(m_depthBuffer);
	renderPass.SetPipelineDescription(m_pipelineDesc);
	renderPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		// Update the transform uniform data
		deviceContext.CopyDataToBuffer(m_uniformBuffer, &m_transform, sizeof(TransformData));
		m_uniformCollection.QueueBufferUpdate(m_uniformBuffer, 0, 0, 0);
		deviceContext.FlushUniformUpdates(m_uniformCollection);
		deviceContext.BindUniformCollection(m_uniformCollection);

		deviceContext.BindMesh(m_vertexBuffer, m_indexBuffer);
		deviceContext.SetScissor({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.SetViewport({ m_swapChain.GetWidth(), m_swapChain.GetHeight() }, { 0, 0 });
		deviceContext.DrawIndexed(static_cast<u32>(cubeAsset->indices.size()));
	});

	m_renderGraph.Bake(m_swapChain);

	// Viz
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 64;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "./BasicModel_RG_%u.dot", frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	m_renderGraph.EndFrame(m_swapChain);
}

void BasicModelSample::InitSample()
{
	STATUS_CODE phxRes;

	m_window.SetWindowTitle("PHX %u.%u.%u | BASIC MODEL", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// LOAD MODEL
	{
		m_assetID = AssetManager::Get().LoadOrImport("suzanne.fbx");
		if (m_assetID == Common::INVALID_ASSET_HANDLE)
		{
			return;
		}
	}
	const AssetType* cubeAsset = AssetManager::Get().GetAsset(m_assetID);

	// VERTEX BUFFER
	const u64 vBufferSizeBytes = static_cast<u64>(cubeAsset->vertices.size() * sizeof(AssetVertex));

	BufferCreateInfo vBufferCI{};
	vBufferCI.pName = "VertexBuffer";
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = vBufferSizeBytes;
	phxRes = m_renderDevice.AllocateBuffer(vBufferCI, m_vertexBuffer);
	CHECK_PHX_RES(phxRes);

	// INDEX BUFFER
	const u64 iBufferSizeBytes = static_cast<u64>(cubeAsset->indices.size() * sizeof(Common::AssetIndexType));

	BufferCreateInfo iBufferCI{};
	iBufferCI.pName = "IndexBuffer";
	iBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	iBufferCI.sizeBytes = iBufferSizeBytes;
	phxRes = m_renderDevice.AllocateBuffer(iBufferCI, m_indexBuffer);
	CHECK_PHX_RES(phxRes);

	// DEPTH BUFFER
	TextureBaseCreateInfo depthBufferBaseCI{};
	depthBufferBaseCI.pName = "DepthBuffer";
	depthBufferBaseCI.width = m_swapChain.GetWidth();
	depthBufferBaseCI.height = m_swapChain.GetHeight();
	depthBufferBaseCI.arrayLayers = 1;
	depthBufferBaseCI.generateMips = false;
	depthBufferBaseCI.format = BASE_FORMAT::D32_FLOAT;
	depthBufferBaseCI.usageFlags = USAGE_TYPE_FLAG_DEPTH_STENCIL_ATTACHMENT | USAGE_TYPE_FLAG_SAMPLED;

	TextureViewCreateInfo depthBufferViewCI{};
	depthBufferViewCI.type = VIEW_TYPE::TYPE_2D;
	depthBufferViewCI.scope = VIEW_SCOPE::ENTIRE;
	depthBufferViewCI.aspectFlags = ASPECT_TYPE_FLAG_DEPTH;

	TextureSamplerCreateInfo depthBufferSamplerCI{};
	depthBufferSamplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::REPEAT;
	depthBufferSamplerCI.enableAnisotropicFiltering = false;
	depthBufferSamplerCI.magnificationFilter = FILTER_MODE::NEAREST;
	depthBufferSamplerCI.minificationFilter = FILTER_MODE::NEAREST;
	depthBufferSamplerCI.samplerMipMapFilter = FILTER_MODE::NEAREST;

	phxRes = m_renderDevice.AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, m_depthBuffer);
	CHECK_PHX_RES(phxRes);

	// SHADERS
	ShaderHandle vertShader;
	vertShader = m_pShaderManager->RegisterShader("../src/shaders/basic.vert", SHADER_STAGE::VERTEX, m_renderDevice);
	if (!vertShader.IsValid())
	{
		return;
	}

	ShaderHandle fragShader;
	fragShader = m_pShaderManager->RegisterShader("../src/shaders/basic.frag", SHADER_STAGE::FRAGMENT, m_renderDevice);
	if (!fragShader.IsValid())
	{
		return;
	}

	m_shaders.push_back(vertShader);
	m_shaders.push_back(fragShader);

	// INPUT ATTRIBUTES
	m_inputAttributes =
	{
		// POSITION
		{
			0,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format
		},
		// NORMAL
		{
			1,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format
		},
	};

	// TRANSFORMS + UNIFORM BUFFER
	glm::vec3 initialCameraPos = { 0.0f, 0.0f, -10.0f };
	float fov = 45.0f;
	float aspectRatio = static_cast<float>(m_window.GetCurrentWidth()) / m_window.GetCurrentHeight();
	m_transform = InitializeTransform(initialCameraPos, fov, aspectRatio);

	BufferCreateInfo uniformBufferCI{};
	uniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	uniformBufferCI.sizeBytes = sizeof(TransformData);
	phxRes = m_renderDevice.AllocateBuffer(uniformBufferCI, m_uniformBuffer);
	CHECK_PHX_RES(phxRes);

	// UNIFORM COLLECTION
	CreateUniformCollection();

	// GRAPHICS PIPELINE
	m_pipelineDesc.viewportSize = { m_window.GetCurrentWidth(), m_window.GetCurrentHeight() };
	m_pipelineDesc.viewportPos = { 0, 0 };
	m_pipelineDesc.polygonMode = POLYGON_MODE::FILL;
	m_pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_pipelineDesc.cullMode = CULL_MODE::BACK;
	m_pipelineDesc.frontFaceWinding = FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
	m_pipelineDesc.pShaders = m_shaders.data();
	m_pipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
	m_pipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_pipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_pipelineDesc.uniformCollection = m_uniformCollection;
	m_pipelineDesc.enableDepthTest = true;
	m_pipelineDesc.enableDepthWrite = true;

	// Upload pass
	UploadMeshDataToGPU();
}

void BasicModelSample::ShutdownSample()
{
	m_shaders.clear();
}

void BasicModelSample::CreateUniformCollection()
{
	UniformData uniformData;
	uniformData.binding = 0;
	uniformData.shaderStage = SHADER_STAGE_FLAG_VERTEX;
	uniformData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup dataGroup;
	dataGroup.set = 1;
	dataGroup.uniformArray = &uniformData;
	dataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = &dataGroup;
	uniformCollectionCI.groupCount = 1;

	STATUS_CODE phxRes = m_renderDevice.AllocateUniformCollection(uniformCollectionCI, m_uniformCollection);
	CHECK_PHX_RES(phxRes);
}

void BasicModelSample::UploadMeshDataToGPU()
{
	RenderPassHandle meshUploadPass;
	STATUS_CODE phxRes = m_renderGraph.RegisterPass("MeshDataUpload", PASS_TYPE::TRANSFER, meshUploadPass);
	CHECK_PHX_RES(phxRes);

	meshUploadPass.SetBufferOutput(m_vertexBuffer);
	meshUploadPass.SetBufferOutput(m_indexBuffer);
	meshUploadPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		const AssetType* cubeAsset = AssetManager::Get().GetAsset(m_assetID);
		const u64 vBufferSizeBytes = static_cast<u64>(cubeAsset->vertices.size() * sizeof(AssetVertex));
		const u64 iBufferSizeBytes = static_cast<u64>(cubeAsset->indices.size() * sizeof(Common::AssetIndexType));

		const AssetType* pAsset = AssetManager::Get().GetAsset(m_assetID);
		deviceContext.CopyDataToBuffer(m_vertexBuffer, pAsset->vertices.data(), vBufferSizeBytes);
		deviceContext.CopyDataToBuffer(m_indexBuffer, pAsset->indices.data(), iBufferSizeBytes);
	});
}
