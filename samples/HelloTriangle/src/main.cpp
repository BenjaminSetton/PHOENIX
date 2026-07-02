
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
static PHX::RenderDeviceHandle s_renderDevice;
static PHX::SwapChainHandle s_swapChain;

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
	s_swapChain.Resize(newWidth, newHeight);
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
	result = CreateRenderDevice(renderDeviceCI, s_renderDevice);
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

	result = s_renderDevice.AllocateSwapChain(swapChainCI, s_swapChain);
	if (result != STATUS_CODE::SUCCESS)
	{
		// Failed to create swap chain
		return -1;
	}

	// SHADERS
	std::string vertShaderName("../src/shaders/vertex_sample.vert");
	ShaderHandle vertShader;
	if (!Common::AllocateShader(vertShaderName, SHADER_STAGE::VERTEX, s_renderDevice, vertShader))
	{
		return -1;
	}

	std::string fragShaderName("../src/shaders/fragment_sample.frag");
	ShaderHandle fragShader;
	if (!Common::AllocateShader(fragShaderName, SHADER_STAGE::FRAGMENT, s_renderDevice, fragShader))
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

	UniformCollectionHandle uniforms;
	result = s_renderDevice.AllocateUniformCollection(uniformCollectionCI, uniforms);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// VERTEX BUFFER
	BufferCreateInfo bufferCI{};
	bufferCI.bufferUsage = PHX::BUFFER_USAGE::VERTEX_BUFFER;
	bufferCI.sizeBytes = sizeof(SimpleVertexType) * VERTEX_COUNT; // Triangle!

	BufferHandle vBuffer;
	result = s_renderDevice.AllocateBuffer(bufferCI, vBuffer);
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

	BufferHandle uniformBuffer;
	result = s_renderDevice.AllocateBuffer(uniformBufferCI, uniformBuffer);
	if (result != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// RENDER GRAPH
	RenderGraphHandle renderGraph;
	result = s_renderDevice.AllocateRenderGraph(renderGraph);
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

	std::array<ShaderHandle, 2> shaders =
	{
		vertShader,
		fragShader
	};

	GraphicsPipelineDesc pipelineDesc{};
	pipelineDesc.topology = PHX::PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineDesc.pInputAttributes = inputAttributes.data();
	pipelineDesc.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineDesc.viewportSize = { s_pWindow->GetCurrentWidth(), s_pWindow->GetCurrentHeight() };
	pipelineDesc.pShaders = shaders.data();
	pipelineDesc.shaderCount = static_cast<u32>(shaders.size());
	pipelineDesc.cullMode = PHX::CULL_MODE::NONE;
	pipelineDesc.uniformCollection = uniforms;

	// Upload mesh to GPU
	RenderPassHandle renderPass;
	result = renderGraph.RegisterPass("MeshDataUpload", BIND_POINT::TRANSFER, renderPass);
	if (result != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	renderPass.SetBufferOutput(vBuffer);
	renderPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
	{
		deviceContext.CopyDataToBuffer(vBuffer, &triVerts, sizeof(SimpleVertexType) * 3);
	});

	// CORE LOOP
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

		s_pWindow->SetWindowTitle("PHX - %s | FRAME %u | FRAMETIME %2.2fms | FPS %2.2f", s_renderDevice.GetDeviceName(), i, elapsedMs.count(), 1.0f / elapsedSeconds.count());

		// Draw operations
		result = renderGraph.BeginFrame(s_swapChain);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to begin frame - skipping frame!" << std::endl;
			continue;
		}

		TextureHandle backbufferTex = s_swapChain.GetCurrentImage();
		RenderPassHandle newPass;
		result = renderGraph.RegisterPass("HelloTriangle", BIND_POINT::GRAPHICS, newPass);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to register new pass - skipping frame!" << std::endl;
			continue;
		}

		newPass.SetBufferInput(vBuffer);
		newPass.SetTextureOutput(backbufferTex, PHX::ATTACHMENT_LOAD_OP::CLEAR, PHX::ATTACHMENT_STORE_OP::STORE, clearCol);
		newPass.SetPipelineDescription(pipelineDesc);
		newPass.SetExecuteCallback([&](DeviceContextHandle deviceContext)
		{
			// Update test UBO
			test.time += elapsedSeconds.count();
			deviceContext.CopyDataToBuffer(uniformBuffer, &test, sizeof(TestUBO));
			uniforms.QueueBufferUpdate(uniformBuffer, 0, 0, 0);
			uniforms.FlushUpdateQueue();

			deviceContext.BindUniformCollection(uniforms);
			deviceContext.BindVertexBuffer(vBuffer);
			deviceContext.SetScissor({ s_swapChain.GetWidth(), s_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.SetViewport({ s_swapChain.GetWidth(), s_swapChain.GetHeight() }, { 0, 0 });
			deviceContext.Draw(VERTEX_COUNT);
		});

		result = renderGraph.Bake(s_swapChain);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to bake render graph - skipping frame!" << std::endl;
			continue;
		}

		result = renderGraph.EndFrame();
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to end frame - skipping frame!" << std::endl;
			continue;
		}

		result = s_swapChain.Present();
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

	// Clean up core objects
	PHX::DestroyWindow(&s_pWindow);
}