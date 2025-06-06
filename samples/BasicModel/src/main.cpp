
#include "PHX/phx.h"

using namespace PHX;

void OnSwapChainOutdatedCallback()
{

}

void OnWindowResizedCallback(PHX::u32 newWidth, PHX::u32 newHeight)
{

}

void OnWindowFocusChangedCallback(bool inFocus)
{

}

void OnWindowMinimizedCallback(bool wasMinimized)
{

}

void OnWindowMaximizedCallback(bool wasMaximized)
{

}

int main(int argc, char** argv)
{
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

	// RENDER GRAPH
	IRenderGraph* pRenderGraph = nullptr;
	phxRes = pRenderDevice->AllocateRenderGraph(&pRenderGraph);
	if (phxRes != STATUS_CODE::SUCCESS)
	{
		return -1;
	}

	ClearValues clearVals{};

	// MAIN LOOP
	while(!pWindow->ShouldClose())
	{
		pWindow->Update(0);

		pRenderGraph->BeginFrame(pSwapChain);
		// do work
		pRenderGraph->EndFrame(pSwapChain);
		pRenderGraph->Bake(pSwapChain, &clearVals, 1);

		pSwapChain->Present();
	}

	pRenderDevice->DeallocateSwapChain(&pSwapChain);

	DestroyWindow(&pWindow);
	DestroyRenderDevice(&pRenderDevice);

	return 0;
}