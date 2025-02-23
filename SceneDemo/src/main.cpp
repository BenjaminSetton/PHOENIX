
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <PHX/phx.h>

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
	swapChainCI.window = pWindow.get();
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
		desc.loadOp = PHX::ATTACHMENT_LOAD_OP::IGNORE;

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

	// PIPELINE
	std::vector<InputAttribute> inputAttributes =
	{
		// POSITION
		{
			0,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format
		},
		{
			0,								// location
			0,								// binding
			BASE_FORMAT::R32G32B32_FLOAT	// format
		},
	};

	std::array<IShader*, 2> shaders =
	{
		pVertShader,
		pFragShader
	};

	PipelineCreateInfo pipelineCI{};
	pipelineCI.type = PHX::PIPELINE_TYPE::GRAPHICS;
	pipelineCI.topology = PHX::PRIMITIVE_TOPOLOGY::TRIANGLE_LIST;
	pipelineCI.enableRestartPrimitives = false;
	pipelineCI.pInputAttributes = inputAttributes.data();
	pipelineCI.attributeCount = static_cast<u32>(inputAttributes.size());
	pipelineCI.inputBinding = 0; // ?????
	pipelineCI.inputRate = PHX::VERTEX_INPUT_RATE::PER_VERTEX;
	pipelineCI.viewportX = 0.0f;
	pipelineCI.viewportY = 0.0f;
	pipelineCI.viewportWidth = static_cast<float>(pWindow->GetCurrentWidth());
	pipelineCI.viewportHeight = static_cast<float>(pWindow->GetCurrentHeight());
	pipelineCI.viewportMinDepth = 0.0f;
	pipelineCI.viewportMaxDepth = FLT_MAX;
	pipelineCI.scissorOffsetX = 0.0f;
	pipelineCI.scissorOffsetY = 0.0f;
	pipelineCI.scissorExtentX = 0.0f;
	pipelineCI.scissorExtentY = 0.0f;
	pipelineCI.enableDepthClamp = true;
	pipelineCI.enableRasterizerDiscard = true;
	pipelineCI.polygonMode = POLYGON_MODE::FILL;
	pipelineCI.cullMode = CULL_MODE::BACK;
	pipelineCI.frontFaceWinding = FRONT_FACE_WINDING::COUNTER_CLOCKWISE;
	pipelineCI.enableDepthBias = false;
	pipelineCI.depthBiasConstantFactor = 0.0f;
	pipelineCI.depthBiasClamp = 0.0f;
	pipelineCI.depthBiasSlopeFactor = 0.0f;
	pipelineCI.lineWidth = 1.0f;
	pipelineCI.rasterizationSamples = PHX::SAMPLE_COUNT::COUNT_1;
	pipelineCI.enableAlphaToCoverage = false;
	pipelineCI.enableAlphaToOne = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = true;
	pipelineCI.compareOp = PHX::COMPARE_OP::LESS_OR_EQUAL;
	pipelineCI.enableDepthBoundsTest = true;
	pipelineCI.enableStencilTest = false;
	pipelineCI.stencilFront = {};
	pipelineCI.stencilBack = {};
	pipelineCI.minDepthBounds = 0.0f;
	pipelineCI.maxDepthBounds = FLT_MAX;
	pipelineCI.pUniformCollection = nullptr;
	pipelineCI.ppShaders = shaders.data();
	pipelineCI.shaderCount = static_cast<u32>(shaders.size());
	pipelineCI.pFramebuffer = framebuffers.at(0);

	IPipeline* pPipeline = nullptr;
	if (pRenderDevice->AllocatePipeline(pipelineCI, &pPipeline) != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// CORE LOOP
	int i = 0;
	std::chrono::duration<float, std::milli> frameBudget(1.0f / 60.0f * 1000.0f); // 60FPS in ms
	while (!pWindow->ShouldClose())
	{
		const auto timeStart = std::chrono::high_resolution_clock::now();

		pWindow->Update(0.13f);

		const auto timeEnd = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<float, std::milli> elapsed = timeEnd - timeStart;

		pWindow->SetWindowTitle("PHX - %s | FRAME %u | FPS %2.2fms", pRenderDevice->GetDeviceName(), i++, elapsed.count());

		// Sleep for the remainder of the frame, if under budget
		if (elapsed < frameBudget)
		{
			const auto timeDiff = frameBudget - elapsed;
			std::this_thread::sleep_for(timeDiff);
			//std::cout << "Slept for " << timeDiff.count() << "ms  out of " << frameBudget.count() << "ms" << std::endl;
		}
	}
}