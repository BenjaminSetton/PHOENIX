
#include <iostream>

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

	int i = 0;
	while (!pWindow->ShouldClose())
	{
		pWindow->Update(0.13f);
		pWindow->SetWindowTitle("PHX - %u", i++);
	}
}