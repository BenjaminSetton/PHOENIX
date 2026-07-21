
#include <iostream>

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
		m_window(), m_renderDevice(), m_swapChain(), m_renderGraph(), m_pCamera(nullptr), m_pShaderManager(nullptr)
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
		settings.backendAPIMajorVersion = 1;
		settings.backendAPIMinorVersion = 2;
		settings.logCallback = nullptr;

		settings.enableValidation = true; // TODO - Add DEBUG project define for samples and guard this setting based on that
		settings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
		settings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
		settings.windowMaximizedCallback = OnWindowMaximizedCallback;
		settings.windowMinimizedCallback = OnWindowMinimizedCallback;
		settings.windowResizedCallback = OnWindowResizedCallback;
		settings.windowKeyDownCallback = [=](KeyCode keycode) { this->OnKeyDown(keycode); };
		settings.windowKeyUpCallback = [=](KeyCode keycode) { this->OnKeyUp(keycode); };
		settings.mouseMovedCallback = [=](float newX, float newY) { this->OnMouseMoved(newX, newY); };
		settings.windowMouseButtonDownCallback = [=](MouseButtonCode mouseButton) { this->OnMouseButtonDown(mouseButton); };
		settings.windowMouseButtonUpCallback = [=](MouseButtonCode mouseButton) { this->OnMouseButtonUp(mouseButton); };
		settings.windowKeyRepeatCallback = nullptr;

		// TODO - Move init calls out of constructor so overridden functions work as expected
		// Allow derived classes to cherry-pick settings to override
		OverrideSettings(settings);

		STATUS_CODE phxRes = PHX::Initialize(settings, m_window);
		CHECK_PHX_RES(phxRes);

		CreateRenderDevice();
		CreateSwapChain();
		CreateRenderGraph();

		m_pShaderManager = new ShaderManager();
	}

	void BaseSample::Shutdown()
	{
		delete m_pShaderManager;
		m_pShaderManager = nullptr;

		STATUS_CODE res = PHX::Shutdown();
		if (res != STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to clean up PHX lib!" << std::endl;
		}
	}

	bool BaseSample::Update(float dt)
	{
		STATUS_CODE res = PHX::Update(dt);
		if (res != STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to update PHX lib!" << std::endl;
			return false; // Keep looping
		}

		m_window.Update(dt);

		InputManager::GetInstance().Update();

		if (m_pCamera != nullptr)
		{
			m_pCamera->Update(dt);
		}

		if (m_pShaderManager != nullptr)
		{
			m_pShaderManager->PollUpdates();
		}

		return m_window.ShouldClose();
	}

	void BaseSample::OverrideSettings(PHX::Settings& settings)
	{
		// unused
		(void)settings;
	}

	void BaseSample::CreateWindow()
	{
		WindowCreateInfo windowCI{};
		windowCI.cursorType = CURSOR_TYPE::SHOWN;
		windowCI.canResize = false;
		windowCI.size = { 2560, 1440 };
		windowCI.position = { 400, 400 };

		STATUS_CODE phxRes = PHX::CreateWindow(windowCI, m_window);
		CHECK_PHX_RES(phxRes);
	}

	void BaseSample::CreateSwapChain()
	{
		SwapChainCreateInfo swapChainCI{};
		swapChainCI.enableVSync = true;
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