
#include <array>
#include <fstream>
#include <sstream>

#include "asset_loader.h"
#include "PHX/phx.h"

using namespace PHX;

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

[[nodiscard]] static PHX::IShader* AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::IRenderDevice* pRenderDevice)
{
	PHX::STATUS_CODE result = PHX::STATUS_CODE::SUCCESS;

	std::ifstream shaderFile;
	shaderFile.open(shaderName, std::ios::in);
	if (!shaderFile.is_open())
	{
		return nullptr;
	}
	std::stringstream buffer;
	buffer << shaderFile.rdbuf();
	std::string shaderStr = buffer.str();

	PHX::ShaderSourceData shaderSrc;
	shaderSrc.data = shaderStr.c_str();
	shaderSrc.stage = stage;
	shaderSrc.origin = PHX::SHADER_ORIGIN::GLSL;
	shaderSrc.optimizationLevel = PHX::SHADER_OPTIMIZATION_LEVEL::NONE;

	PHX::CompiledShader shaderRes;
	result = CompileShader(shaderSrc, shaderRes);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return nullptr;
	}

	PHX::ShaderCreateInfo shaderCI{};
	shaderCI.pBytecode = shaderRes.data.get();
	shaderCI.size = shaderRes.size;
	shaderCI.stage = stage;

	PHX::IShader* pShader = nullptr;
	result = pRenderDevice->AllocateShader(shaderCI, &pShader);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return nullptr;
	}

	return pShader;
}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	// Load model
	Common::AssetHandle cubeAssetID = Common::INVALID_ASSET_HANDLE;
	{
		std::shared_ptr<Common::AssetDisk> assetDisk = Common::ImportAsset("../../common/assets/cube.fbx");
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
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}
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
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// RENDER DEVICE
	RenderDeviceCreateInfo renderDeviceCI{};
	renderDeviceCI.framesInFlight = 2;
	renderDeviceCI.window = pWindow;

	IRenderDevice* pRenderDevice = nullptr;
	phxRes = CreateRenderDevice(renderDeviceCI, &pRenderDevice);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = pWindow->GetCurrentWidth();
	swapChainCI.height = pWindow->GetCurrentHeight();

	ISwapChain* pSwapChain = nullptr;
	phxRes = pRenderDevice->AllocateSwapChain(swapChainCI, &pSwapChain);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// VERTEX BUFFER
	const u64 vBufferSizeBytes = static_cast<u64>(cubeAsset->vertices.size() * sizeof(AssetVertex));

	BufferCreateInfo vBufferCI{};
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = vBufferSizeBytes;

	IBuffer* pVertexBuffer = nullptr;
	phxRes = pRenderDevice->AllocateBuffer(vBufferCI, &pVertexBuffer);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// Copy over asset vertex data to GPU, and keep it there permanently :)
	phxRes = pVertexBuffer->CopyData(cubeAsset->vertices.data(), vBufferSizeBytes);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// INDEX BUFFER
	const u64 iBufferSizeBytes = static_cast<u64>(cubeAsset->indices.size() * sizeof(Common::AssetIndexType));

	BufferCreateInfo iBufferCI{};
	iBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	iBufferCI.sizeBytes = iBufferSizeBytes;

	IBuffer* pIndexBuffer = nullptr;
	phxRes = pRenderDevice->AllocateBuffer(iBufferCI, &pIndexBuffer);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// Copy over asset vertex data to GPU, and keep it there permanently :)
	phxRes = pIndexBuffer->CopyData(cubeAsset->indices.data(), iBufferSizeBytes);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// RENDER GRAPH
	IRenderGraph* pRenderGraph = nullptr;
	phxRes = pRenderDevice->AllocateRenderGraph(&pRenderGraph);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// SHADERS
	IShader* pVertShader = AllocateShader("../src/shaders/basic.vert", SHADER_STAGE::VERTEX, pRenderDevice);
	IShader* pFragShader = AllocateShader("../src/shaders/basic.frag", SHADER_STAGE::FRAGMENT, pRenderDevice);

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

	// GRAPHICS PIPELINE
	GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.viewportSize = { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() };
	pipelineDesc.viewportPos = { 0, 0 };
	pipelineDesc.polygonMode = POLYGON_MODE::FILL;
	pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineDesc.cullMode = CULL_MODE::NONE;
	pipelineDesc.ppShaders = shaders.data();
	pipelineDesc.shaderCount = static_cast<u32>(shaders.size());
	pipelineDesc.pInputAttributes = inputAttributes.data();
	pipelineDesc.attributeCount = static_cast<u32>(inputAttributes.size());

	ClearValues clearVals{};

	// MAIN LOOP
	while(!pWindow->ShouldClose())
	{
		pWindow->Update(0);

		pRenderGraph->BeginFrame(pSwapChain);
		
		IRenderPass* pRenderPass = pRenderGraph->RegisterPass("BasicCubePass", BIND_POINT::GRAPHICS);
		pRenderPass->SetBackbufferOutput(pSwapChain->GetCurrentImage());
		pRenderPass->SetPipeline(pipelineDesc);
		pRenderPass->SetExecuteCallback([&](IDeviceContext* pContext, IPipeline* pPipeline) {

			pContext->CopyDataToBuffer(pVertexBuffer, cubeAsset->vertices.data(), vBufferSizeBytes);
			pContext->CopyDataToBuffer(pIndexBuffer, cubeAsset->indices.data(), iBufferSizeBytes);
			pContext->BindMesh(pVertexBuffer, pIndexBuffer);
			pContext->BindPipeline(pPipeline);
			pContext->SetScissor( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			pContext->SetViewport( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			pContext->DrawIndexed(static_cast<u32>(cubeAsset->indices.size()));
		});

		pRenderGraph->Bake(pSwapChain, &clearVals, 1);
		pRenderGraph->EndFrame(pSwapChain);

		pSwapChain->Present();
	}

	pRenderDevice->DeallocateBuffer(&pIndexBuffer);
	pRenderDevice->DeallocateBuffer(&pVertexBuffer);
	pRenderDevice->DeallocateSwapChain(&pSwapChain);

	DestroyWindow(&pWindow);
	DestroyRenderDevice(&pRenderDevice);

	return 0;
}