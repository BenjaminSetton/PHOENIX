
#include <array>
#include <fstream>
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <sstream>

#include "../../common/src/utils/shader_utils.h"
#include "asset_loader.h"
#include "PHX/phx.h"

using namespace PHX;

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return -1; }

struct TransformData
{
	glm::mat4 worldMat;
	glm::mat4 viewMat;
	glm::mat4 projMat;
};

void OnSwapChainOutdatedCallback()
{

}

void OnWindowResizedCallback(PHX::u32 newWidth, PHX::u32 newHeight)
{
	(void)newWidth;
	(void)newHeight;
}

void OnWindowFocusChangedCallback(bool inFocus)
{
	(void)inFocus;
}

void OnWindowMinimizedCallback(bool wasMinimized)
{
	(void)wasMinimized;
}

void OnWindowMaximizedCallback(bool wasMaximized)
{
	(void)wasMaximized;
}

static TransformData InitializeTransform(glm::vec3 initialCameraPos, float FOV, float aspectRatio)
{
	TransformData data;

	// World
	data.worldMat = glm::identity<glm::mat4>();

	// View (toward -Z)
	glm::vec3 eye = initialCameraPos;
	glm::vec3 center = { 0.0f, 0.0f, -1.0f };
	glm::vec3 up = { 0.0f, 1.0f, 0.0f };
	data.viewMat =  glm::inverse(glm::lookAt(eye, center, up));

	// Perspective
	data.projMat = glm::perspective(FOV, aspectRatio, 0.01f, 1000.0f);

	return data;
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	// Load model
	Common::AssetHandle cubeAssetID = Common::INVALID_ASSET_HANDLE;
	{
		std::shared_ptr<Common::AssetDisk> assetDisk = Common::ImportAsset("../../common/assets/suzanne.fbx");
		cubeAssetID = ConvertAssetDiskToAssetType(assetDisk.get());
	}
	const AssetType* cubeAsset = AssetManager::Get().GetAsset(cubeAssetID);

	STATUS_CODE phxRes;

	WindowCreateInfo windowCI{};
	windowCI.cursorType = CURSOR_TYPE::SHOWN;
	windowCI.canResize = false;

	// WINDOW
	IWindow* pWindow;
	phxRes = CreateWindow(windowCI, &pWindow);
	CHECK_PHX_RES(phxRes);

	pWindow->SetWindowTitle("PHX %u.%u.%u | BASIC MODEL", PHX::GetMajorVersion(), PHX::GetMinorVersion(), PHX::GetPatchVersion());

	// INIT
	Settings settings{};
	settings.backendAPI = GRAPHICS_API::VULKAN;
	settings.enableValidation = true;
	settings.logCallback = nullptr;
	settings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
	settings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
	settings.windowMaximizedCallback = OnWindowMaximizedCallback;
	settings.windowMinimizedCallback = OnWindowMinimizedCallback;
	settings.windowResizedCallback = OnWindowResizedCallback;
	phxRes = Initialize(settings, pWindow);
	CHECK_PHX_RES(phxRes);

	// RENDER DEVICE
	RenderDeviceCreateInfo renderDeviceCI{};
	renderDeviceCI.framesInFlight = 2;
	renderDeviceCI.window = pWindow;

	IRenderDevice* pRenderDevice = nullptr;
	phxRes = CreateRenderDevice(renderDeviceCI, &pRenderDevice);
	CHECK_PHX_RES(phxRes);

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = pWindow->GetCurrentWidth();
	swapChainCI.height = pWindow->GetCurrentHeight();

	ISwapChain* pSwapChain = nullptr;
	phxRes = pRenderDevice->AllocateSwapChain(swapChainCI, &pSwapChain);
	CHECK_PHX_RES(phxRes);

	// VERTEX BUFFER
	const u64 vBufferSizeBytes = static_cast<u64>(cubeAsset->vertices.size() * sizeof(AssetVertex));

	BufferCreateInfo vBufferCI{};
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = vBufferSizeBytes;

	IBuffer* pVertexBuffer = nullptr;
	phxRes = pRenderDevice->AllocateBuffer(vBufferCI, &pVertexBuffer);
	CHECK_PHX_RES(phxRes);

	// INDEX BUFFER
	const u64 iBufferSizeBytes = static_cast<u64>(cubeAsset->indices.size() * sizeof(Common::AssetIndexType));

	BufferCreateInfo iBufferCI{};
	iBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	iBufferCI.sizeBytes = iBufferSizeBytes;

	IBuffer* pIndexBuffer = nullptr;
	phxRes = pRenderDevice->AllocateBuffer(iBufferCI, &pIndexBuffer);
	CHECK_PHX_RES(phxRes);

	// RENDER GRAPH
	IRenderGraph* pRenderGraph = nullptr;
	phxRes = pRenderDevice->AllocateRenderGraph(&pRenderGraph);
	CHECK_PHX_RES(phxRes);

	// DEPTH BUFFER
	TextureBaseCreateInfo depthBufferBaseCI{};
	depthBufferBaseCI.width = pWindow->GetCurrentWidth();
	depthBufferBaseCI.height = pWindow->GetCurrentHeight();
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

	ITexture* pDepthBuffer = nullptr;
	phxRes = pRenderDevice->AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, &pDepthBuffer);
	CHECK_PHX_RES(phxRes);

	// SHADERS
	IShader* pVertShader = Common::AllocateShader("../src/shaders/basic.vert", SHADER_STAGE::VERTEX, pRenderDevice);
	IShader* pFragShader = Common::AllocateShader("../src/shaders/basic.frag", SHADER_STAGE::FRAGMENT, pRenderDevice);

	std::array<IShader*, 2> shaders =
	{
		pVertShader,
		pFragShader
	};

	std::vector<InputAttribute> inputAttributes =
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
	glm::vec3 initialCameraPos = { 0.0f, 1.0f, -7.0f };
	float fov = 45.0f;
	float aspectRatio = static_cast<float>(pWindow->GetCurrentWidth()) / pWindow->GetCurrentHeight();
	TransformData transform = InitializeTransform(initialCameraPos, fov, aspectRatio);

	BufferCreateInfo uniformBufferCI{};
	uniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	uniformBufferCI.sizeBytes = sizeof(TransformData);

	IBuffer* pUniformBuffer = nullptr;
	phxRes = pRenderDevice->AllocateBuffer(uniformBufferCI, &pUniformBuffer);
	CHECK_PHX_RES(phxRes);

	UniformData uniformData;
	uniformData.binding = 0;
	uniformData.shaderStage = SHADER_STAGE::VERTEX;
	uniformData.type = UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup dataGroup;
	dataGroup.set = 1;
	dataGroup.uniformArray = &uniformData;
	dataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = &dataGroup;
	uniformCollectionCI.groupCount = 1;

	IUniformCollection* pUniformCollection = nullptr;
	phxRes = pRenderDevice->AllocateUniformCollection(uniformCollectionCI, &pUniformCollection);
	CHECK_PHX_RES(phxRes);

	// GRAPHICS PIPELINE
	GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.viewportSize = { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() };
	pipelineDesc.viewportPos = { 0, 0 };
	pipelineDesc.polygonMode = POLYGON_MODE::FILL;
	pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineDesc.cullMode = CULL_MODE::FRONT;
	pipelineDesc.ppShaders = shaders.data();
	pipelineDesc.shaderCount = static_cast<u32>(shaders.size());
	pipelineDesc.pInputAttributes = inputAttributes.data();
	pipelineDesc.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineDesc.pUniformCollection = pUniformCollection;
	pipelineDesc.enableDepthTest = true;
	pipelineDesc.enableDepthWrite = true;

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

	bool updatedMeshBufferData = false;

	// MAIN LOOP
	while(!pWindow->ShouldClose())
	{
		pWindow->Update(0);

		pRenderGraph->BeginFrame(pSwapChain);

		// Update the cube's transform
		transform.worldMat = glm::rotate(transform.worldMat, 0.02f, { 0.0f, -1.0f, 0.0f });
		transform.worldMat = glm::rotate(transform.worldMat, 0.02f, { 1.0f, 0.0f, 0.0f });
		
		IRenderPass* pRenderPass = pRenderGraph->RegisterPass("BasicCubePass", BIND_POINT::GRAPHICS);
		pRenderPass->SetBackbufferOutput(pSwapChain->GetCurrentImage());
		pRenderPass->SetDepthOutput(pDepthBuffer);
		pRenderPass->SetPipeline(pipelineDesc);
		pRenderPass->SetExecuteCallback([&](IDeviceContext* pContext, IPipeline* pPipeline)
		{
			if (!updatedMeshBufferData)
			{
				pContext->CopyDataToBuffer(pVertexBuffer, cubeAsset->vertices.data(), vBufferSizeBytes);
				pContext->CopyDataToBuffer(pIndexBuffer, cubeAsset->indices.data(), iBufferSizeBytes);

				updatedMeshBufferData = true;
			}

			// Update the transform uniform data
			pContext->CopyDataToBuffer(pUniformBuffer, &transform, sizeof(TransformData));
			pUniformCollection->QueueBufferUpdate(0, 0, 0, pUniformBuffer);
			pUniformCollection->FlushUpdateQueue();
			pContext->BindUniformCollection(pUniformCollection, pPipeline);

			pContext->BindMesh(pVertexBuffer, pIndexBuffer);
			pContext->BindPipeline(pPipeline);
			pContext->SetScissor( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			pContext->SetViewport( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			pContext->DrawIndexed(static_cast<u32>(cubeAsset->indices.size()));
		});

		pRenderGraph->Bake(pSwapChain, clearVals.data(), static_cast<u32>(clearVals.size()));
		pRenderGraph->EndFrame(pSwapChain);

		pSwapChain->Present();
	}

	pRenderDevice->DeallocateBuffer(&pUniformBuffer);
	pRenderDevice->DeallocateBuffer(&pIndexBuffer);
	pRenderDevice->DeallocateBuffer(&pVertexBuffer);
	pRenderDevice->DeallocateUniformCollection(&pUniformCollection);
	pRenderDevice->DeallocateTexture(&pDepthBuffer);
	pRenderDevice->DeallocateShader(&pVertShader);
	pRenderDevice->DeallocateShader(&pFragShader);
	pRenderDevice->DeallocateSwapChain(&pSwapChain);

	DestroyWindow(&pWindow);
	DestroyRenderDevice(&pRenderDevice);

	return 0;
}