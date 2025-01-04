
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include <PHX/phx.h>

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
		if (pRenderDevice->AllocateFramebuffer(framebufferCI, framebuffers.at(i)) != STATUS_CODE::SUCCESS)
		{
			return -1;
		}
	}

	// SHADERS
	std::ifstream shaderFile;
	shaderFile.open("../src/shaders/vertex_sample.vert", std::ios::in);
	if (!shaderFile.is_open())
	{
		return -1;
	}
	std::stringstream buffer;
	buffer << shaderFile.rdbuf();
	std::string shaderStr = buffer.str();

	ShaderSourceData shaderSrc;
	shaderSrc.data = shaderStr.c_str();
	shaderSrc.name = "vertex_sample";
	shaderSrc.kind = PHX::SHADER_KIND::VERTEX;
	shaderSrc.origin = PHX::SHADER_ORIGIN::GLSL;
	shaderSrc.optimizationLevel = PHX::SHADER_OPTIMIZATION_LEVEL::NONE;

	CompiledShader shaderRes;
	if (CompileShader(shaderSrc, shaderRes) != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	ShaderCreateInfo shaderCI{};
	shaderCI.pBytecode = shaderRes.data.get();
	shaderCI.size = shaderRes.size;
	shaderCI.type = PHX::SHADER_KIND::VERTEX;

	IShader* pShader = nullptr;
	if (pRenderDevice->AllocateShader(shaderCI, pShader) != PHX::STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	// CORE LOOP
	int i = 0;
	while (!pWindow->ShouldClose())
	{
		pWindow->Update(0.13f);
		pWindow->SetWindowTitle("PHX - %s - %u", pRenderDevice->GetDeviceName(), i++);
	}
}