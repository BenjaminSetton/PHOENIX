
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <PHX/phx.h>

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

[[nodiscard]] static PHX::IShader* AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, std::shared_ptr<PHX::IRenderDevice> pRenderDevice)
{
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
	if (CompileShader(shaderSrc, shaderRes) != PHX::STATUS_CODE::SUCCESS)
	{
		return nullptr;
	}

	PHX::ShaderCreateInfo shaderCI{};
	shaderCI.pBytecode = shaderRes.data.get();
	shaderCI.size = shaderRes.size;
	shaderCI.stage = stage;

	PHX::IShader* pShader = nullptr;
	if (pRenderDevice->AllocateShader(shaderCI, &pShader) != PHX::STATUS_CODE::SUCCESS)
	{
		return nullptr;
	}

	return pShader;
}

int main(int argc, char** argv)
{
	using namespace PHX;
	(void)argc;
	(void)argv;

	// WINDOW
	WindowCreateInfo windowCI{};
	if (CreateWindow(windowCI) != STATUS_CODE::SUCCESS)
	{
		// Failed to create window
		return -1;
	}

	auto pWindow = GetWindow();

	// CORE GRAPHICS
	if (InitializeGraphics(GRAPHICS_API::VULKAN) != STATUS_CODE::SUCCESS)
	{
		// Failed to initialize graphics
		return -1;
	}

	// RENDER DEVICE
	RenderDeviceCreateInfo renderDeviceCI{};
	renderDeviceCI.window = GetWindow().get();
	if (CreateRenderDevice(renderDeviceCI) != STATUS_CODE::SUCCESS)
	{
		// Failed to create render device
		return -1;
	}

	auto pRenderDevice = GetRenderDevice();

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = pWindow->GetCurrentWidth();
	swapChainCI.height = pWindow->GetCurrentHeight();
	swapChainCI.renderDevice = pRenderDevice.get();
	if (CreateSwapChain(swapChainCI) != STATUS_CODE::SUCCESS)
	{
		// Failed to create swap chain
		return -1;
	}

	auto pSwapChain = GetSwapChain();

	// FRAMEBUFFER
	std::vector<IFramebuffer*> framebuffers(pSwapChain->GetImageCount());
	for (u32 i = 0; i < pSwapChain->GetImageCount(); i++)
	{
		FramebufferAttachmentDesc desc;
		desc.pTexture = pSwapChain->GetImage(i);
		desc.mipTarget = 0;
		desc.type = PHX::ATTACHMENT_TYPE::COLOR;
		desc.storeOp = PHX::ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = PHX::ATTACHMENT_LOAD_OP::CLEAR;

		FramebufferCreateInfo framebufferCI{};
		framebufferCI.width = desc.pTexture->GetWidth();
		framebufferCI.height = desc.pTexture->GetHeight();
		framebufferCI.layers = 1;
		framebufferCI.pAttachments = &desc;
		framebufferCI.attachmentCount = 1;
		if (pRenderDevice->AllocateFramebuffer(framebufferCI, &(framebuffers.at(i))) != STATUS_CODE::SUCCESS)
		{
			return -1;
		}
	}

	// SHADERS
	std::string vertShaderName("../src/shaders/vertex_sample.vert");
	IShader* pVertShader = AllocateShader(vertShaderName, SHADER_STAGE::VERTEX, pRenderDevice);
	if (pVertShader == nullptr)
	{
		return -1;
	}

	std::string fragShaderName("../src/shaders/fragment_sample.frag");
	IShader* pFragShader = AllocateShader(fragShaderName, SHADER_STAGE::FRAGMENT, pRenderDevice);
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
	if (pRenderDevice->AllocateUniformCollection(uniformCollectionCI, &pUniforms) != PHX::STATUS_CODE::SUCCESS)
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

	GraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.topology = PHX::PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineCI.pInputAttributes = inputAttributes.data();
	pipelineCI.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineCI.viewportSize = { pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() };
	pipelineCI.ppShaders = shaders.data();
	pipelineCI.shaderCount = static_cast<u32>(shaders.size());
	pipelineCI.pFramebuffer = framebuffers.at(0);
	pipelineCI.cullMode = PHX::CULL_MODE::NONE;
	pipelineCI.pUniformCollection = pUniforms;

	IPipeline* pPipeline = nullptr;
	if (pRenderDevice->AllocateGraphicsPipeline(pipelineCI, &pPipeline) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// DEVICE CONTEXT
	IDeviceContext* pDeviceContext = nullptr;
	if (pRenderDevice->AllocateDeviceContext({}, &pDeviceContext) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// VERTEX BUFFER
	BufferCreateInfo bufferCI{};
	bufferCI.bufferUsage = PHX::BUFFER_USAGE::VERTEX_BUFFER;
	bufferCI.size = sizeof(SimpleVertexType) * VERTEX_COUNT; // Triangle!

	IBuffer* vBuffer = nullptr;
	if (pRenderDevice->AllocateBuffer(bufferCI, &vBuffer) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// Copy over the vertex data to the vertex buffer
	if (pDeviceContext->CopyDataToBuffer(vBuffer, &triVerts, sizeof(SimpleVertexType) * VERTEX_COUNT) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	//for (u32 i = 0; i < pSwapChain->GetImageCount(); i++)
	//{
	//	if (pDeviceContext->TEMP_TransitionTextureToGeneralLayout(pSwapChain->GetImage(i)) != PHX::STATUS_CODE::SUCCESS)
	//	{
	//		return -1;
	//	}
	//}

	// Just clear the color attachment, no depth/stencil
	ClearValues clearCol{};
	clearCol.color.color = { 0.1f, 0.1f, 0.1f, 0.0f };
	clearCol.isClearColor = true;

	// UNIFORM BUFFER
	TestUBO test{};
	test.time = 0.0f;

	BufferCreateInfo uniformBufferCI{};
	uniformBufferCI.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER;
	uniformBufferCI.size = sizeof(TestUBO);

	IBuffer* uniformBuffer = nullptr;
	if (pRenderDevice->AllocateBuffer(uniformBufferCI, &uniformBuffer) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// CORE LOOP
	int i = 0;
	std::chrono::duration<float> frameBudgetMs(1.0f / 60.0f); // 60FPS
	auto timeStart = std::chrono::high_resolution_clock::now();
	auto timeEnd = std::chrono::high_resolution_clock::now();
	while (!pWindow->ShouldClose())
	{
		const std::chrono::duration<float, std::milli> elapsedMs = timeEnd - timeStart;
		const std::chrono::duration<float> elapsedSeconds = timeEnd - timeStart;

		timeStart = std::chrono::high_resolution_clock::now();

		pWindow->Update(0.13f);

		pWindow->SetWindowTitle("PHX - %s | FRAME %u | FRAMETIME %2.2fms | FPS %2.2f", pRenderDevice->GetDeviceName(), i++, elapsedMs.count(), 1.0f / elapsedSeconds.count());

		// Draw operations
		pDeviceContext->BeginFrame(pSwapChain.get());

		u32 currSwapChainImageIndex = i % pSwapChain->GetImageCount();

		auto& currFramebuffer = framebuffers.at(currSwapChainImageIndex);
		pDeviceContext->BeginRenderPass(currFramebuffer, &clearCol, 1);

		// Update test UBO
		test.time += elapsedSeconds.count();
		uniformBuffer->CopyData(&test, sizeof(TestUBO));

		pUniforms->QueueBufferUpdate(0, 0, 0, uniformBuffer);
		pUniforms->FlushUpdateQueue();

		// Represents recording one secondary command buffer
		{
			pDeviceContext->BindPipeline(pPipeline);
			pDeviceContext->BindUniformCollection(pUniforms, pPipeline);
			pDeviceContext->BindVertexBuffer(vBuffer);
			pDeviceContext->SetScissor({ pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 });
			pDeviceContext->SetViewport({ pWindow->GetCurrentWidth(), pWindow->GetCurrentHeight() }, { 0, 0 });
			pDeviceContext->Draw(VERTEX_COUNT);
		}

		pDeviceContext->EndRenderPass();

		pDeviceContext->Flush();

		//pDeviceContext->TEMP_TransitionTextureToPresentLayout(pSwapChain->GetImage(currSwapChainImageIndex));

		pSwapChain->Present();

		// Sleep for the remainder of the frame, if under budget
		if (elapsedMs < frameBudgetMs)
		{
			const auto timeDiff = frameBudgetMs - elapsedMs;
			std::this_thread::sleep_for(timeDiff);
		}

		timeEnd = std::chrono::high_resolution_clock::now();
	}
}