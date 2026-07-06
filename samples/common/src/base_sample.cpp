
#include <iostream> // TEMP

#include "base_sample.h"

#include "input_manager.h"

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
		m_window(), m_renderDevice(), m_swapChain(), m_renderGraph(), m_pCamera(nullptr)
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
		settings.backendAPI                    = GRAPHICS_API::VULKAN;
		settings.logCallback                   = nullptr;

#if defined(DEBUG)
		settings.enableValidation              = true;
#else
		settings.enableValidation              = false;
#endif
		settings.swapChainOutdatedCallback     = OnSwapChainOutdatedCallback;
		settings.windowFocusChangedCallback    = OnWindowFocusChangedCallback;
		settings.windowMaximizedCallback       = OnWindowMaximizedCallback;
		settings.windowMinimizedCallback       = OnWindowMinimizedCallback;
		settings.windowResizedCallback         = OnWindowResizedCallback;
		settings.windowKeyDownCallback         = [=](KeyCode keycode) { this->OnKeyDown(keycode); };
		settings.windowKeyUpCallback           = [=](KeyCode keycode) { this->OnKeyUp(keycode); };
		settings.mouseMovedCallback            = [=](float newX, float newY) { this->OnMouseMoved(newX, newY); };
		settings.windowMouseButtonDownCallback = [=](MouseButtonCode mouseButton) { this->OnMouseButtonDown(mouseButton); };
		settings.windowMouseButtonUpCallback   = [=](MouseButtonCode mouseButton) { this->OnMouseButtonUp(mouseButton); };
		settings.windowKeyRepeatCallback       = nullptr;

		STATUS_CODE phxRes = PHX::Initialize(settings, m_window);
		CHECK_PHX_RES(phxRes);

		CreateRenderDevice();
		CreateSwapChain();
		CreateRenderGraph();
	}

	void BaseSample::Shutdown()
	{
	}

	bool BaseSample::Update(float dt)
	{
		m_window.Update(dt);

		InputManager::GetInstance().Update();

		if (m_pCamera != nullptr)
		{
			m_pCamera->Update(dt);
		}

		return m_window.ShouldClose();
	}

	void BaseSample::CreateWindow()
	{
		WindowCreateInfo windowCI{};
		windowCI.cursorType = CURSOR_TYPE::SHOWN;
		windowCI.canResize = false;

		STATUS_CODE phxRes = PHX::CreateWindow(windowCI, m_window);
		CHECK_PHX_RES(phxRes);
	}

	void BaseSample::CreateSwapChain()
	{
		SwapChainCreateInfo swapChainCI{};
		swapChainCI.enableVSync = false;
		swapChainCI.width = m_window.GetCurrentWidth();
		swapChainCI.height = m_window.GetCurrentHeight();

		STATUS_CODE phxRes = m_renderDevice.AllocateSwapChain(swapChainCI, m_swapChain);
		CHECK_PHX_RES(phxRes);
	}

	void BaseSample::CreateRenderDevice()
	{
		RenderDeviceCreateInfo renderDeviceCI{};
		renderDeviceCI.framesInFlight = 2;
		renderDeviceCI.window = m_window;

		STATUS_CODE phxRes = PHX::CreateRenderDevice(renderDeviceCI, m_renderDevice);
		CHECK_PHX_RES(phxRes);
	}

	void BaseSample::CreateRenderGraph()
	{
		STATUS_CODE phxRes = m_renderDevice.AllocateRenderGraph(m_renderGraph);
		CHECK_PHX_RES(phxRes);
	}

	void BaseSample::OnKeyDown(PHX::KeyCode keycode)
	{
		InputManager::GetInstance().SetKeyCode(keycode, true);
	}

	void BaseSample::OnKeyUp(PHX::KeyCode keycode)
	{
		InputManager::GetInstance().SetKeyCode(keycode, false);
	}

	void BaseSample::OnMouseButtonDown(PHX::MouseButtonCode mouseButton)
	{
		InputManager::GetInstance().SetMouseButton(mouseButton, true);
	}

	void BaseSample::OnMouseButtonUp(PHX::MouseButtonCode mouseButton)
	{
		InputManager::GetInstance().SetMouseButton(mouseButton, false);
	}

	void BaseSample::OnMouseMoved(float newX, float newY)
	{
		InputManager::GetInstance().SetMousePosition(newX, newY);
	}
}