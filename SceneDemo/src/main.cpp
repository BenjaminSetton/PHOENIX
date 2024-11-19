
#include <iostream>

#include <PHX/phx.h>

int main(int argc, char** argv)
{
	using namespace PHX;
	(void)argc;
	(void)argv;

	ObjectFactory& factory = ObjectFactory::Get();
	factory.SelectGraphicsAPI(PHX::GRAPHICS_API::VULKAN);

	// WINDOW
	WindowCreateInfo windowCI{};
	std::shared_ptr<IWindow> pWindow = factory.CreateWindow(windowCI);

	// RENDER DEVICE
	RenderDeviceCreateInfo renderDeviceCI{};
	renderDeviceCI.window = pWindow.get();
	std::shared_ptr<IRenderDevice> pRenderDevice = factory.CreateRenderDevice(renderDeviceCI);
	pRenderDevice->AllocateBuffer();

	// SWAP CHAIN
	SwapChainCreateInfo swapChainCI{};
	swapChainCI.enableVSync = true;
	swapChainCI.width = pWindow->GetCurrentWidth();
	swapChainCI.height = pWindow->GetCurrentHeight();
	swapChainCI.renderDevice = pRenderDevice.get();
	swapChainCI.window = pWindow.get();
	std::shared_ptr<ISwapChain> pSwapChain = factory.CreateSwapChain(swapChainCI);

	int i = 0;
	while (!pWindow->ShouldClose())
	{
		pWindow->Update(0.13f);
		pWindow->SetWindowTitle("PHX - %u", i++);
	}
}