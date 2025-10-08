
#include <array>
#include <fstream>
#include <gtc/matrix_transform.hpp>
#include <sstream>

#include "textured_model_sample.h"

#include "../../common/src/utils/shader_utils.h"
#include "../../common/src/camera/freefly_camera.h"
#include "asset_loader.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

static TransformData InitializeTransform(const Common::BaseCamera* pCamera, float FOV, float aspectRatio, float scale)
{
	TransformData data;

	// World
	data.worldMat = glm::identity<glm::mat4>();
	data.worldMat = glm::scale(data.worldMat, glm::vec3(scale));

	// View (toward -Z)
	if (pCamera != nullptr)
	{
		data.viewMat = pCamera->GetViewMatrix();
	}
	else
	{
		glm::vec3 defaultEye = { 0.0f, 0.0f, 0.0f };
		glm::vec3 defaultCenter = { 0.0f, 0.0f, 1.0f };
		data.viewMat = glm::lookAt(defaultEye, defaultCenter, { 0.0f, 1.0f, 0.0f });
	}

	// Perspective
	data.projMat = glm::perspective(FOV, aspectRatio, 0.01f, 1000.0f);

	return data;
}

TexturedModelSample::TexturedModelSample() : m_transform(), m_pipelineDesc(), 
	m_pDepthBuffer(nullptr), m_pUniformCollection(nullptr), m_pTransformBuffer(nullptr), m_pCameraBuffer(nullptr),
	m_pVertexBuffer(nullptr), m_pIndexBuffer(nullptr), m_assetID(Common::INVALID_ASSET_HANDLE)
{
	Init();
}

TexturedModelSample::~TexturedModelSample()
{
	Shutdown();
}

bool TexturedModelSample::Update(float dt)
{
	// Update the asset's transform
	//m_transform.worldMat = glm::rotate(m_transform.worldMat, 0.01f, { 0.0f, -1.0f, 0.0f });

	// Get the new view matrix from the camera
	m_transform.viewMat = m_pCamera->GetViewMatrix();

	return BaseSample::Update(dt);
}

void TexturedModelSample::Draw()
{
	const AssetType* axeAsset = AssetManager::Get().GetAsset(m_assetID);
	if (axeAsset == nullptr)
	{
		return;
	}

	ClearValues clearColor{};
	clearColor.color.color = Vec4f(0.5f, 0.75f, 0.98f, 1.0f);
	clearColor.useClearColor = true;

	ClearValues clearDepth{};
	clearDepth.depthStencil.depthClear = 1.0f;
	clearDepth.depthStencil.stencilClear = 0;
	clearDepth.useClearColor = false;

	std::array<ClearValues, 2> clearVals =
	{
		clearColor,
		clearDepth
	};

	m_pRenderGraph->BeginFrame(m_pSwapChain);

	// Setup a new render pass for PBR
	IRenderPass* pRenderPass = m_pRenderGraph->RegisterPass("PBRPass", BIND_POINT::GRAPHICS);
	pRenderPass->SetBackbufferOutput(m_pSwapChain->GetCurrentImage());
	pRenderPass->SetDepthOutput(m_pDepthBuffer);

	for (ITexture* assetTex : m_assetTextures)
	{
		pRenderPass->SetTextureInput(assetTex);
	}
	pRenderPass->SetBufferInput(m_pVertexBuffer);
	pRenderPass->SetBufferInput(m_pIndexBuffer);

	pRenderPass->SetPipelineDescription(m_pipelineDesc);
	pRenderPass->SetExecuteCallback([&](IDeviceContext* pContext, IPipeline* pPipeline)
	{
		// Uniform collection updates
		pContext->CopyDataToBuffer(m_pTransformBuffer, &m_transform, sizeof(TransformData));
		m_pUniformCollection->QueueBufferUpdate(m_pTransformBuffer, 0, 0, 0);

		for (u32 i = 0; i < m_assetTextures.size(); i++)
		{
			ITexture* pCurrTex = m_assetTextures[i];
			m_pUniformCollection->QueueImageUpdate(pCurrTex, 1, i, 0);
		}

		CameraData cameraData{};
		cameraData.cameraPos = m_pCamera->GetPosition();
		pContext->CopyDataToBuffer(m_pCameraBuffer, &cameraData, sizeof(CameraData));
		m_pUniformCollection->QueueBufferUpdate(m_pCameraBuffer, 2, 0, 0);

		m_pUniformCollection->FlushUpdateQueue();

		// Draw commands
		pContext->BindUniformCollection(m_pUniformCollection, pPipeline);
		pContext->BindMesh(m_pVertexBuffer, m_pIndexBuffer);
		pContext->BindPipeline(pPipeline);
		pContext->SetScissor({ m_pWindow->GetCurrentWidth(), m_pWindow->GetCurrentHeight() }, { 0, 0 });
		pContext->SetViewport({ m_pWindow->GetCurrentWidth(), m_pWindow->GetCurrentHeight() }, { 0, 0 });
		pContext->DrawIndexed(static_cast<u32>(axeAsset->indices.size()));
	});

	m_pRenderGraph->Bake(clearVals.data(), static_cast<u32>(clearVals.size()));
	m_pRenderGraph->EndFrame();

	m_pSwapChain->Present();
}

void TexturedModelSample::Init()
{
	STATUS_CODE phxRes;

	m_pWindow->SetWindowTitle("PHX %u.%u.%u | TEXTURED MODEL", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// LOAD MODEL
	{
		std::shared_ptr<Common::AssetDisk> assetDisk = Common::ImportAsset("../assets/axe/scene.gltf");
		m_assetID = ConvertAssetDiskToAssetType(assetDisk.get());
	}
	const AssetType* axeAsset = AssetManager::Get().GetAsset(m_assetID);

	// VERTEX BUFFER
	const u64 vBufferSizeBytes = static_cast<u64>(axeAsset->vertices.size() * sizeof(AssetVertex));

	BufferCreateInfo vBufferCI{};
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = vBufferSizeBytes;

	phxRes = m_pRenderDevice->AllocateBuffer(vBufferCI, &m_pVertexBuffer);
	CHECK_PHX_RES(phxRes);

	// INDEX BUFFER
	const u64 iBufferSizeBytes = static_cast<u64>(axeAsset->indices.size() * sizeof(Common::AssetIndexType));

	BufferCreateInfo iBufferCI{};
	iBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	iBufferCI.sizeBytes = iBufferSizeBytes;

	phxRes = m_pRenderDevice->AllocateBuffer(iBufferCI, &m_pIndexBuffer);
	CHECK_PHX_RES(phxRes);

	// DEPTH BUFFER
	TextureBaseCreateInfo depthBufferBaseCI{};
	depthBufferBaseCI.width = m_pWindow->GetCurrentWidth();
	depthBufferBaseCI.height = m_pWindow->GetCurrentHeight();
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

	phxRes = m_pRenderDevice->AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, &m_pDepthBuffer);
	CHECK_PHX_RES(phxRes);

	// SHADERS
	IShader* pVertShader = Common::AllocateShader("../src/shaders/basic.vert", SHADER_STAGE::VERTEX, m_pRenderDevice);
	if (pVertShader == nullptr)
	{
		return;
	}

	IShader* pFragShader = Common::AllocateShader("../src/shaders/basic.frag", SHADER_STAGE::FRAGMENT, m_pRenderDevice);
	if (pFragShader == nullptr)
	{
		return;
	}

	m_shaders.push_back(pVertShader);
	m_shaders.push_back(pFragShader);

	// TODO - Replace with data from vertex shader reflection
	// INPUT ATTRIBUTES
	m_inputAttributes =
	{
		// POSITION (12 bytes)
		{
			0,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format (Vec3f)
		},
		// NORMAL (12 bytes)
		{
			1,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format (Vec3f)
		},
		// TANGENT (12 bytes)
		{
			2,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format (Vec3f)
		},
		// BITANGENT (12 bytes)
		{
			3,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format (Vec3f)
		},
		// UV (8 bytes)
		{
			4,								// location
			0,								// binding
			BASE_FORMAT::R32G32_FLOAT		// format (Vec2f)
		},
	};

	// Create a new freefly camera
	const float cameraSpeed = 4.0f;
	const float cameraSensitivity = 0.2f;
	m_pCamera = new Common::FreeflyCamera(cameraSpeed, cameraSensitivity);

	// TRANSFORMS + UNIFORM BUFFER
	const float fov = 45.0f;
	const float aspectRatio = static_cast<float>(m_pWindow->GetCurrentWidth()) / m_pWindow->GetCurrentHeight();
	const float scale = 0.005f;
	m_transform = InitializeTransform(m_pCamera, fov, aspectRatio, scale);

	BufferCreateInfo transformUniformBufferCI{};
	transformUniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	transformUniformBufferCI.sizeBytes = sizeof(TransformData);

	phxRes = m_pRenderDevice->AllocateBuffer(transformUniformBufferCI, &m_pTransformBuffer);
	CHECK_PHX_RES(phxRes);

	BufferCreateInfo cameraUniformBufferCI{};
	cameraUniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	cameraUniformBufferCI.sizeBytes = sizeof(CameraData);

	phxRes = m_pRenderDevice->AllocateBuffer(cameraUniformBufferCI, &m_pCameraBuffer);
	CHECK_PHX_RES(phxRes);

	// UNIFORM DATA
	CreateUniformCollection();

	// GRAPHICS PIPELINE
	m_pipelineDesc.viewportSize = { m_pWindow->GetCurrentWidth(), m_pWindow->GetCurrentHeight() };
	m_pipelineDesc.viewportPos = { 0, 0 };
	m_pipelineDesc.polygonMode = POLYGON_MODE::FILL;
	m_pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	m_pipelineDesc.cullMode = CULL_MODE::FRONT;
	m_pipelineDesc.ppShaders = m_shaders.data();
	m_pipelineDesc.shaderCount = static_cast<u32>(m_shaders.size());
	m_pipelineDesc.pInputAttributes = m_inputAttributes.data();
	m_pipelineDesc.attributeCount = static_cast<u32>(m_inputAttributes.size());
	m_pipelineDesc.pUniformCollection = m_pUniformCollection;
	m_pipelineDesc.enableDepthTest = true;
	m_pipelineDesc.enableDepthWrite = true;

	// ASSET TEXTURES
	CreateAssetTextures();

	// Create a new render pass to upload the mesh data to the GPU
	UploadMeshDataToGPU();
}

void TexturedModelSample::Shutdown()
{
	for (ITexture* pAssetTex : m_assetTextures)
	{
		m_pRenderDevice->DeallocateTexture(&pAssetTex);
	}
	m_assetTextures.clear();

	for (IShader* pShader : m_shaders)
	{
		m_pRenderDevice->DeallocateShader(&pShader);
	}
	m_shaders.clear();

	m_pRenderDevice->DeallocateBuffer(&m_pCameraBuffer);
	m_pRenderDevice->DeallocateBuffer(&m_pTransformBuffer);
	m_pRenderDevice->DeallocateBuffer(&m_pIndexBuffer);
	m_pRenderDevice->DeallocateBuffer(&m_pVertexBuffer);
	m_pRenderDevice->DeallocateUniformCollection(&m_pUniformCollection);
	m_pRenderDevice->DeallocateTexture(&m_pDepthBuffer);

	delete m_pCamera;
	m_pCamera = nullptr;
}

void TexturedModelSample::CreateAssetTextures()
{
	AssetType* pAsset = AssetManager::Get().GetAsset(m_assetID);
	if (pAsset == nullptr)
	{
		return;
	}

	for (u32 i = 0; i < static_cast<u32>(pAsset->textures.size()); i++)
	{
		const Texture& currTex = pAsset->textures[i];

		TextureBaseCreateInfo baseCI{};
		baseCI.width = currTex.size.GetX();
		baseCI.height = currTex.size.GetY();
		baseCI.arrayLayers = 1;
		baseCI.generateMips = false;
		baseCI.format = (currTex.type == Common::TEXTURE_TYPE::DIFFUSE) ? BASE_FORMAT::R8G8B8A8_SRGB : BASE_FORMAT::R8G8B8A8_UNORM;
		baseCI.usageFlags = USAGE_TYPE_FLAG_SAMPLED | USAGE_TYPE_FLAG_TRANSFER_DST | USAGE_TYPE_FLAG_INPUT_ATTACHMENT;

		TextureViewCreateInfo viewCI{};
		viewCI.type = VIEW_TYPE::TYPE_2D;
		viewCI.scope = VIEW_SCOPE::ENTIRE;
		viewCI.aspectFlags = ASPECT_TYPE_FLAG_COLOR;

		TextureSamplerCreateInfo samplerCI{};
		samplerCI.addressModeUVW = SAMPLER_ADDRESS_MODE::REPEAT;
		samplerCI.enableAnisotropicFiltering = false;
		samplerCI.magnificationFilter = FILTER_MODE::LINEAR;
		samplerCI.minificationFilter = FILTER_MODE::LINEAR;
		samplerCI.samplerMipMapFilter = FILTER_MODE::LINEAR;

		ITexture* pAssetTex = nullptr;
		STATUS_CODE res = m_pRenderDevice->AllocateTexture(baseCI, viewCI, samplerCI, &pAssetTex);
		CHECK_PHX_RES(res);

		m_assetTextures.push_back(pAssetTex);
	}
}

void TexturedModelSample::CreateUniformCollection()
{
	// SET 0
	UniformData transformUniformData;
	transformUniformData.binding = 0;
	transformUniformData.shaderStage = SHADER_STAGE::VERTEX;
	transformUniformData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup transformDataGroup;
	transformDataGroup.set = 0;
	transformDataGroup.uniformArray = &transformUniformData;
	transformDataGroup.uniformArrayCount = 1;

	// SET 1
	std::vector<UniformData> texUniforms;
	for (u32 i = 0; i < 5; i++)
	{
		UniformData texUniformData;
		texUniformData.binding = i;
		texUniformData.shaderStage = SHADER_STAGE::FRAGMENT;
		texUniformData.type = UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER;

		texUniforms.push_back(texUniformData);
	}

	UniformDataGroup texUniformDataGroup;
	texUniformDataGroup.set = 1;
	texUniformDataGroup.uniformArray = texUniforms.data();
	texUniformDataGroup.uniformArrayCount = static_cast<u32>(texUniforms.size());

	// SET 2
	UniformData cameraUniformData;
	cameraUniformData.binding = 0;
	cameraUniformData.shaderStage = SHADER_STAGE::FRAGMENT;
	cameraUniformData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup cameraDataGroup;
	cameraDataGroup.set = 2;
	cameraDataGroup.uniformArray = &cameraUniformData;
	cameraDataGroup.uniformArrayCount = 1;

	std::array<UniformDataGroup, 3> dataGroups =
	{
		transformDataGroup,
		texUniformDataGroup,
		cameraDataGroup
	};

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = dataGroups.data();
	uniformCollectionCI.groupCount = static_cast<u32>(dataGroups.size());

	STATUS_CODE phxRes = m_pRenderDevice->AllocateUniformCollection(uniformCollectionCI, &m_pUniformCollection);
	CHECK_PHX_RES(phxRes);
}

void TexturedModelSample::UploadMeshDataToGPU()
{
	IRenderPass* pRenderPass = m_pRenderGraph->RegisterPass("MeshDataUpload", BIND_POINT::TRANSFER);
	pRenderPass->SetBufferOutput(m_pVertexBuffer);
	pRenderPass->SetBufferOutput(m_pIndexBuffer);
	for (u32 i = 0; i < m_assetTextures.size(); i++)
	{
		ITexture* pCurrAssetTex = m_assetTextures[i];
		pRenderPass->SetColorOutput(pCurrAssetTex);
	}

	pRenderPass->SetExecuteCallback([&](IDeviceContext* pContext, IPipeline* pPipeline)
	{
		// Unused
		(void)pPipeline;

		const AssetType* pAsset = AssetManager::Get().GetAsset(m_assetID);
		const u64 vBufferSizeBytes = static_cast<u64>(pAsset->vertices.size() * sizeof(AssetVertex));
		const u64 iBufferSizeBytes = static_cast<u64>(pAsset->indices.size() * sizeof(Common::AssetIndexType));

		pContext->CopyDataToBuffer(m_pVertexBuffer, pAsset->vertices.data(), vBufferSizeBytes);
		pContext->CopyDataToBuffer(m_pIndexBuffer, pAsset->indices.data(), iBufferSizeBytes);

		for (u32 i = 0; i < m_assetTextures.size(); i++)
		{
			const Texture& texSrc = pAsset->textures[i];
			ITexture* texDst = m_assetTextures[i];
			u64 sizeBytes = (texSrc.size.GetX() * texSrc.size.GetY() * texSrc.bytesPerPixel);
			pContext->CopyDataToTexture(texDst, texSrc.data, sizeBytes);
		}
	});
}