
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

	RenderDeviceHandle renderDevice;
	phxRes = CreateRenderDevice(renderDeviceCI, renderDevice);
	CHECK_PHX_RES(phxRes);

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = pWindow->GetCurrentWidth();
	swapChainCI.height = pWindow->GetCurrentHeight();

	SwapChainHandle swapChain;
	phxRes = renderDevice.AllocateSwapChain(swapChainCI, swapChain);
	CHECK_PHX_RES(phxRes);

	// VERTEX BUFFER
	const u64 vBufferSizeBytes = static_cast<u64>(cubeAsset->vertices.size() * sizeof(AssetVertex));

	BufferCreateInfo vBufferCI{};
	vBufferCI.pName = "VertexBuffer";
	vBufferCI.bufferUsage = BUFFER_USAGE::VERTEX_BUFFER;
	vBufferCI.sizeBytes = vBufferSizeBytes;

	BufferHandle vertexBuffer;
	phxRes = renderDevice.AllocateBuffer(vBufferCI, vertexBuffer);
	CHECK_PHX_RES(phxRes);

	// INDEX BUFFER
	const u64 iBufferSizeBytes = static_cast<u64>(cubeAsset->indices.size() * sizeof(Common::AssetIndexType));

	BufferCreateInfo iBufferCI{};
	iBufferCI.pName = "IndexBuffer";
	iBufferCI.bufferUsage = BUFFER_USAGE::INDEX_BUFFER;
	iBufferCI.sizeBytes = iBufferSizeBytes;

	BufferHandle indexBuffer;
	phxRes = renderDevice.AllocateBuffer(iBufferCI, indexBuffer);
	CHECK_PHX_RES(phxRes);

	// RENDER GRAPH
	RenderGraphHandle renderGraph;
	phxRes = renderDevice.AllocateRenderGraph(renderGraph);
	CHECK_PHX_RES(phxRes);

	// DEPTH BUFFER
	TextureBaseCreateInfo depthBufferBaseCI{};
	depthBufferBaseCI.pName = "DepthBuffer";
	depthBufferBaseCI.width = swapChain.GetWidth();
	depthBufferBaseCI.height = swapChain.GetHeight();
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

	TextureHandle depthBuffer;
	phxRes = renderDevice.AllocateTexture(depthBufferBaseCI, depthBufferViewCI, depthBufferSamplerCI, depthBuffer);
	CHECK_PHX_RES(phxRes);

	// SHADERS
	ShaderHandle vertShader;
	if (!Common::AllocateShader("../src/shaders/basic.vert", SHADER_STAGE::VERTEX, renderDevice, vertShader))
	{
		return -1;
	}

	ShaderHandle fragShader;
	if (!Common::AllocateShader("../src/shaders/basic.frag", SHADER_STAGE::FRAGMENT, renderDevice, fragShader))
	{
		return -1;
	}

	std::array<ShaderHandle, 2> shaders =
	{
		vertShader,
		fragShader
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

	BufferHandle uniformBuffer;
	phxRes = renderDevice.AllocateBuffer(uniformBufferCI, uniformBuffer);
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

	UniformCollectionHandle uniformCollection;
	phxRes = renderDevice.AllocateUniformCollection(uniformCollectionCI, uniformCollection);
	CHECK_PHX_RES(phxRes);

	// GRAPHICS PIPELINE
	GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.viewportSize = { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() };
	pipelineDesc.viewportPos = { 0, 0 };
	pipelineDesc.polygonMode = POLYGON_MODE::FILL;
	pipelineDesc.topology = PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineDesc.cullMode = CULL_MODE::FRONT;
	pipelineDesc.pShaders = shaders.data();
	pipelineDesc.shaderCount = static_cast<u32>(shaders.size());
	pipelineDesc.pInputAttributes = inputAttributes.data();
	pipelineDesc.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineDesc.uniformCollection = uniformCollection;
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

	// Upload mesh to GPU
	RenderPassHandle meshUploadPass;
	phxRes = renderGraph.RegisterPass("MeshDataUpload", BIND_POINT::TRANSFER, meshUploadPass);
	CHECK_PHX_RES(phxRes);

	meshUploadPass.SetBufferOutput(vertexBuffer);
	meshUploadPass.SetBufferOutput(indexBuffer);
	meshUploadPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(vertexBuffer, cubeAsset->vertices.data(), vBufferSizeBytes);
		deviceContext.CopyDataToBuffer(indexBuffer, cubeAsset->indices.data(), iBufferSizeBytes);
	});

	// MAIN LOOP
	while(!pWindow->ShouldClose())
	{
		pWindow->Update(0);

		renderGraph.BeginFrame(swapChain);

		// Update the cube's transform
		transform.worldMat = glm::rotate(transform.worldMat, 0.02f, { 0.0f, -1.0f, 0.0f });
		transform.worldMat = glm::rotate(transform.worldMat, 0.02f, { 1.0f, 0.0f, 0.0f });
		
		RenderPassHandle renderPass;
		phxRes = renderGraph.RegisterPass("BasicCubePass", BIND_POINT::GRAPHICS, renderPass);
		CHECK_PHX_RES(phxRes);

		renderPass.SetBufferInput(vertexBuffer);
		renderPass.SetBufferInput(indexBuffer);
		renderPass.SetBackbufferOutput(swapChain.GetCurrentImage());
		renderPass.SetDepthOutput(depthBuffer);
		renderPass.SetPipelineDescription(pipelineDesc);
		renderPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Update the transform uniform data
			deviceContext.CopyDataToBuffer(uniformBuffer, &transform, sizeof(TransformData));
			uniformCollection.QueueBufferUpdate(uniformBuffer, 0, 0, 0);
			uniformCollection.FlushUpdateQueue();
			deviceContext.BindUniformCollection(uniformCollection);

			deviceContext.BindMesh(vertexBuffer, indexBuffer);
			deviceContext.SetScissor( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			deviceContext.SetViewport( { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 } );
			deviceContext.DrawIndexed(static_cast<u32>(cubeAsset->indices.size()));
		});

		renderGraph.Bake(clearVals.data(), static_cast<u32>(clearVals.size()));
		renderGraph.EndFrame();

		swapChain.Present();
	}

	PHX::DestroyWindow(&pWindow);

	return 0;
}