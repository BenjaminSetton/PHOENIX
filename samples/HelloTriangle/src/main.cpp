
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <PHX/phx.h>

#include "../../common/src/utils/shader_utils.h"

struct SimpleVertexType
{
	float pos[4];
	float color[4];
};

static constexpr PHX::u32 VERTEX_COUNT = 3;
static constexpr SimpleVertexType triVerts[VERTEX_COUNT] =
{
	{{ -0.5f, 0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }},
	{{ 0.0f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }},
	{{ 0.5f, 0.5f, 0.0f, 1.0f } , { 0.0f, 0.0f, 1.0f, 1.0f }}
};

struct TestUBO
{
	float time;
};

// Ugly, but easier for demo's sake
static PHX::IWindow* s_pWindow = nullptr;
static PHX::IRenderDevice* s_pRenderDevice = nullptr;
static PHX::ISwapChain* s_pSwapChain = nullptr;

void LogCallback(const char* msg, PHX::LOG_TYPE severity)
{
	std::cout << "[APP]";
	switch (severity)
	{
	case PHX::LOG_TYPE::ERR:
	{
		std::cout << "[ERROR] ";
		break;
	}
	case PHX::LOG_TYPE::WARNING:
	{
		std::cout << "[WARNING] ";
		break;
	}
	case PHX::LOG_TYPE::INFO:
	{
		std::cout << "[INFO] ";
		break;
	}
	case PHX::LOG_TYPE::DBG:
	{
		std::cout << "[DEBUG] ";
		break;
	}
	default:
	{
		break;
	}
	}

	std::cout << msg << std::endl;
}

void OnWindowResizedCallback(PHX::u32 newWidth, PHX::u32 newHeight)
{
	s_pSwapChain->Resize(newWidth, newHeight);
}

void OnWindowFocusChangedCallback(bool inFocus)
{
	(void)inFocus;
	// TODO
}

void OnWindowMinimizedCallback(bool wasMinimized)
{
	(void)wasMinimized;
	// TODO
}

void OnWindowMaximizedCallback(bool wasMaximized)
{
	(void)wasMaximized;
	// TODO
}

void OnSwapChainOutdatedCallback()
{
	// TODO
}

int main(int argc, char** argv)
{
	using namespace PHX;
	(void)argc;
	(void)argv;

	PHX::STATUS_CODE result = PHX::STATUS_CODE::SUCCESS;

	// WINDOW
	WindowCreateInfo windowCI{};
	result = CreateWindow(windowCI, &s_pWindow);
	if (result != STATUS_CODE::SUCCESS)
	{
		// Failed to create window
		return -1;
	}

	// CORE GRAPHICS
	Settings initSettings{};
	initSettings.backendAPI = PHX::GRAPHICS_API::VULKAN;
	initSettings.enableValidation = true;
	initSettings.logCallback = nullptr; // LogCallback;
	initSettings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
	initSettings.windowResizedCallback = OnWindowResizedCallback;
	initSettings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
	initSettings.windowMinimizedCallback = OnWindowMinimizedCallback;
	initSettings.windowMaximizedCallback = OnWindowMaximizedCallback;
	result = Initialize(initSettings, s_pWindow);
	if (result != STATUS_CODE::SUCCESS)
	{
		// Failed to initialize graphics
		return -1;
	}

	// RENDER DEVICE
	RenderDeviceCreateInfo renderDeviceCI{};
	renderDeviceCI.window = s_pWindow;
	renderDeviceCI.framesInFlight = 3;
	result = CreateRenderDevice(renderDeviceCI, &s_pRenderDevice);
	if (result != STATUS_CODE::SUCCESS)
	{
		// Failed to create render device
		return -1;
	}

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = s_pWindow->GetCurrentWidth();
	swapChainCI.height = s_pWindow->GetCurrentHeight();

	s_pRenderDevice->AllocateSwapChain(swapChainCI, &s_pSwapChain);
	if (result != STATUS_CODE::SUCCESS)
	{
		// Failed to create swap chain
		return -1;
	}

	// SHADERS
	std::string vertShaderName("../src/shaders/vertex_sample.vert");
	IShader* pVertShader = Common::AllocateShader(vertShaderName, SHADER_STAGE::VERTEX, s_pRenderDevice);
	if (pVertShader == nullptr)
	{
		return -1;
	}

	std::string fragShaderName("../src/shaders/fragment_sample.frag");
	IShader* pFragShader = Common::AllocateShader(fragShaderName, SHADER_STAGE::FRAGMENT, s_pRenderDevice);
	if (pFragShader == nullptr)
	{
		return -1;
	}

	// UNIFORM BUFFERS
	UniformData uniform{};
	uniform.binding = 0;
	uniform.shaderStage = PHX::SHADER_STAGE::FRAGMENT;
	uniform.type = PHX::UNIFORM_TYPE::UNIFORM_BUFFER;

	UniformDataGroup uniformDataGroup{};
	uniformDataGroup.set = 0;
	uniformDataGroup.uniformArray = &uniform;
	uniformDataGroup.uniformArrayCount = 1;

	UniformCollectionCreateInfo uniformCollectionCI{};
	uniformCollectionCI.dataGroups = &uniformDataGroup;
	uniformCollectionCI.groupCount = 1;

	IUniformCollection* pUniforms = nullptr;
	result = s_pRenderDevice->AllocateUniformCollection(uniformCollectionCI, &pUniforms);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// VERTEX BUFFER
	BufferCreateInfo bufferCI{};
	bufferCI.bufferUsage = PHX::BUFFER_USAGE::VERTEX_BUFFER;
	bufferCI.sizeBytes = sizeof(SimpleVertexType) * VERTEX_COUNT; // Triangle!

	IBuffer* vBuffer = nullptr;
	result = s_pRenderDevice->AllocateBuffer(bufferCI, &vBuffer);
	if (result != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// Just clear the color attachment, no depth/stencil
	ClearValues clearCol{};
	clearCol.color.color = { 0.1f, 0.1f, 0.1f, 0.0f };
	clearCol.useClearColor = true;

	// UNIFORM BUFFER
	TestUBO test{};
	test.time = 0.0f;

	BufferCreateInfo uniformBufferCI{};
	uniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	uniformBufferCI.sizeBytes = sizeof(TestUBO);

	IBuffer* uniformBuffer = nullptr;
	result = s_pRenderDevice->AllocateBuffer(uniformBufferCI, &uniformBuffer);
	if (result != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// RENDER GRAPH
	IRenderGraph* pRenderGraph = nullptr;
	result = s_pRenderDevice->AllocateRenderGraph(&pRenderGraph);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// PIPELINE
	std::vector<InputAttribute> inputAttributes =
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

	std::array<IShader*, 2> shaders =
	{
		pVertShader,
		pFragShader
	};

	GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.topology = PHX::PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineDesc.pInputAttributes = inputAttributes.data();
	pipelineDesc.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineDesc.viewportSize = { s_pWindow->GetCurrentWidth(), s_pWindow->GetCurrentHeight() };
	pipelineDesc.ppShaders = shaders.data();
	pipelineDesc.shaderCount = static_cast<u32>(shaders.size());
	pipelineDesc.cullMode = PHX::CULL_MODE::NONE;
	pipelineDesc.pUniformCollection = pUniforms;

	// CORE LOOP
	bool updatedMeshBufferData = false;
	int i = 0;
	std::chrono::duration<float> frameBudgetMs(1.0f / 60.0f); // 60FPS
	auto timeStart = std::chrono::high_resolution_clock::now();
	auto timeEnd = std::chrono::high_resolution_clock::now();
	while (!s_pWindow->ShouldClose())
	{
		const std::chrono::duration<float, std::milli> elapsedMs = timeEnd - timeStart;
		const std::chrono::duration<float> elapsedSeconds = timeEnd - timeStart;

		timeStart = std::chrono::high_resolution_clock::now();

		s_pWindow->Update(0.13f);
		if (s_pWindow->IsMinimized())
		{
			// No need to render/draw while minimized
			continue;
		}

		s_pWindow->SetWindowTitle("PHX - %s | FRAME %u | FRAMETIME %2.2fms | FPS %2.2f", s_pRenderDevice->GetDeviceName(), i, elapsedMs.count(), 1.0f / elapsedSeconds.count());

		// Draw operations
		result = pRenderGraph->BeginFrame(s_pSwapChain);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to begin frame - skipping frame!" << std::endl;
			continue;
		}

		ITexture* backbufferTex = s_pSwapChain->GetCurrentImage();
		IRenderPass* newPass = pRenderGraph->RegisterPass("HelloTriangle", BIND_POINT::GRAPHICS);
		newPass->SetBackbufferOutput(backbufferTex);
		newPass->SetPipeline(pipelineDesc);
		newPass->SetExecuteCallback([&](IDeviceContext* pDeviceContext, IPipeline* pPipeline)
		{
			if (!updatedMeshBufferData)
			{
				pDeviceContext->CopyDataToBuffer(vBuffer, &triVerts, sizeof(SimpleVertexType) * VERTEX_COUNT);

				updatedMeshBufferData = true;
			}

			// Update test UBO
			test.time += elapsedSeconds.count();
			pDeviceContext->CopyDataToBuffer(uniformBuffer, &test, sizeof(TestUBO));
			pUniforms->QueueBufferUpdate(0, 0, 0, uniformBuffer);
			pUniforms->FlushUpdateQueue();

			pDeviceContext->BindPipeline(pPipeline);
			pDeviceContext->BindUniformCollection(pUniforms, pPipeline);
			pDeviceContext->BindVertexBuffer(vBuffer);
			pDeviceContext->SetScissor({ s_pWindow->GetCurrentWidth(), s_pWindow->GetCurrentHeight() }, { 0, 0 });
			pDeviceContext->SetViewport({ s_pWindow->GetCurrentWidth(), s_pWindow->GetCurrentHeight() }, { 0, 0 });
			pDeviceContext->Draw(VERTEX_COUNT);
		});

		result = pRenderGraph->Bake(s_pSwapChain, &clearCol, 1);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to bake render graph - skipping frame!" << std::endl;
			continue;
		}

		result = pRenderGraph->EndFrame(s_pSwapChain);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to end frame - skipping frame!" << std::endl;
			continue;
		}

		result = s_pSwapChain->Present();
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to present frame - skipping frame!" << std::endl;
			continue;
		}

		// Sleep for the remainder of the frame, if under budget
		if (elapsedMs < frameBudgetMs)
		{
			const auto timeDiff = frameBudgetMs - elapsedMs;
			std::this_thread::sleep_for(timeDiff);
		}

		timeEnd = std::chrono::high_resolution_clock::now();

		i++;
	}

	// Clean up
	s_pRenderDevice->DeallocateRenderGraph(&pRenderGraph);
	s_pRenderDevice->DeallocateBuffer(&uniformBuffer);
	s_pRenderDevice->DeallocateBuffer(&vBuffer);
	s_pRenderDevice->DeallocateSwapChain(&s_pSwapChain);
	s_pRenderDevice->DeallocateShader(&pVertShader);
	s_pRenderDevice->DeallocateShader(&pFragShader);
	s_pRenderDevice->DeallocateUniformCollection(&pUniforms);

	// Clean up core objects
	DestroyRenderDevice(&s_pRenderDevice);
	DestroyWindow(&s_pWindow);
}