
#include "base_sample.h"

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

using namespace PHX;

namespace Common
{
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

	BaseSample::BaseSample() : 
		m_pWindow(nullptr), m_pSwapChain(nullptr), m_pRenderDevice(nullptr), m_pRenderGraph(nullptr)
	{
		Init();
	}

	BaseSample::~BaseSample()
	{
		Shutdown();
	}

	void BaseSample::Init()
	{
		CreateWindow();

		Settings settings{};
		settings.backendAPI = GRAPHICS_API::VULKAN;
		settings.enableValidation = true;
		settings.logCallback = nullptr;
		settings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
		settings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
		settings.windowMaximizedCallback = OnWindowMaximizedCallback;
		settings.windowMinimizedCallback = OnWindowMinimizedCallback;
		settings.windowResizedCallback = OnWindowResizedCallback;
		STATUS_CODE phxRes = PHX::Initialize(settings, m_pWindow);
		CHECK_PHX_RES(phxRes);

		CreateRenderDevice();
		CreateSwapChain();
		CreateRenderGraph();
	}

	void BaseSample::Shutdown()
	{
		DestroyRenderGraph();
		DestroySwapChain();
		DestroyRenderDevice();
		DestroyWindow();
	}

	bool BaseSample::Update(float dt)
	{
		m_pWindow->Update(dt);
		return m_pWindow->ShouldClose();
	}

	void BaseSample::CreateWindow()
	{
		WindowCreateInfo windowCI{};
		windowCI.cursorType = CURSOR_TYPE::SHOWN;
		windowCI.canResize = false;

		IWindow* pWindow;
		STATUS_CODE phxRes = PHX::CreateWindow(windowCI, &pWindow);
		CHECK_PHX_RES(phxRes);

		m_pWindow = pWindow;
	}

	void BaseSample::CreateSwapChain()
	{
		SwapChainCreateInfo swapChainCI{};
		swapChainCI.enableVSync = true;
		swapChainCI.width = m_pWindow->GetCurrentWidth();
		swapChainCI.height = m_pWindow->GetCurrentHeight();

		ISwapChain* pSwapChain = nullptr;
		STATUS_CODE phxRes = m_pRenderDevice->AllocateSwapChain(swapChainCI, &pSwapChain);
		CHECK_PHX_RES(phxRes);

		m_pSwapChain = pSwapChain;
	}

	void BaseSample::CreateRenderDevice()
	{
		RenderDeviceCreateInfo renderDeviceCI{};
		renderDeviceCI.framesInFlight = 2;
		renderDeviceCI.window = m_pWindow;

		IRenderDevice* pRenderDevice = nullptr;
		STATUS_CODE phxRes = PHX::CreateRenderDevice(renderDeviceCI, &pRenderDevice);
		CHECK_PHX_RES(phxRes);

		m_pRenderDevice = pRenderDevice;
	}

	void BaseSample::CreateRenderGraph()
	{
		IRenderGraph* pRenderGraph = nullptr;
		STATUS_CODE phxRes = m_pRenderDevice->AllocateRenderGraph(&pRenderGraph);
		CHECK_PHX_RES(phxRes);

		m_pRenderGraph = pRenderGraph;
	}

	void BaseSample::DestroyWindow()
	{
		PHX::DestroyWindow(&m_pWindow);
	}

	void BaseSample::DestroySwapChain()
	{
		if (m_pRenderDevice != nullptr)
		{
			m_pRenderDevice->DeallocateSwapChain(&m_pSwapChain);
		}
	}

	void BaseSample::DestroyRenderDevice()
	{
		PHX::DestroyRenderDevice(&m_pRenderDevice);
	}

	void BaseSample::DestroyRenderGraph()
	{
		if (m_pRenderDevice != nullptr)
		{
			m_pRenderDevice->DeallocateRenderGraph(&m_pRenderGraph);
		}
	}
}