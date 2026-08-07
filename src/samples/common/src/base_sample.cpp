
#include <cstdio>
#include <iostream>

#include "base_sample.h"
#include "input_manager.h"

#ifndef CACHE_ROOT_DIR
	#define CACHE_ROOT_DIR "."
#endif

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

using namespace PHX;

namespace Common
{
	void OnSwapChainOutdatedCallback()
	{

	}

	void OnWindowResizedCallback(u32 newWidth, u32 newHeight)
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

	BaseSample::BaseSample() : m_window(), m_renderDevice(), m_swapChain(), m_renderGraph(),
		m_pCamera(nullptr), m_pShaderManager(nullptr), m_imguiInitialized(false)
	{
	}

	BaseSample::~BaseSample()
	{
	}

	void BaseSample::Init()
	{
		CreateWindow();

		Settings settings{};
		settings.backendAPI = GRAPHICS_API::VULKAN;
		settings.backendAPIMajorVersion = 1;
		settings.backendAPIMinorVersion = 0;
		settings.logCallback = nullptr;

		settings.enableValidation = true; // TODO - Add DEBUG project define for samples and guard this setting based on that
		settings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
		settings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
		settings.windowMaximizedCallback = OnWindowMaximizedCallback;
		settings.windowMinimizedCallback = OnWindowMinimizedCallback;
		settings.windowResizedCallback = OnWindowResizedCallback;
		settings.gatherMetrics = true;
		settings.windowKeyDownCallback = [=](KeyCode keycode) { this->OnKeyDown(keycode); };
		settings.windowKeyUpCallback = [=](KeyCode keycode) { this->OnKeyUp(keycode); };
		settings.mouseMovedCallback = [=](float newX, float newY) { this->OnMouseMoved(newX, newY); };
		settings.mouseButtonDownCallback = [=](MouseButtonCode mouseButton) { this->OnMouseButtonDown(mouseButton); };
		settings.mouseButtonUpCallback = [=](MouseButtonCode mouseButton) { this->OnMouseButtonUp(mouseButton); };
		settings.mouseScrollCallback = [=](float scrollX, float scrollY) { this->OnMouseScroll(scrollX, scrollY); };
		settings.windowKeyRepeatCallback = nullptr;

		// Allow derived classes to cherry-pick settings to override
		OverrideSettings(settings);

		STATUS_CODE phxRes = PHX::Initialize(settings, m_window);
		CHECK_PHX_RES(phxRes);

		CreateRenderDevice();
		CreateSwapChain();
		CreateRenderGraph();

		m_pShaderManager = new ShaderManager();

		// ImGui (always available to derived samples)
		if (!m_imguiBackend.Init())
		{
			std::cout << "Failed to initialize ImGui backend!" << std::endl;
			return;
		}
		if (!m_imguiRenderer.Init(m_renderDevice, m_swapChain, m_pShaderManager))
		{
			std::cout << "Failed to initialize ImGui renderer!" << std::endl;
			return;
		}
		m_imguiInitialized = true;

		InitSample();
	}

	void BaseSample::Shutdown()
	{
		STATUS_CODE res = PHX::Shutdown();
		if (res != STATUS_CODE::SUCCESS)
		{
			std::cout << "Failed to clean up PHX lib!" << std::endl;
		}

		ShutdownSample();

		if (m_imguiInitialized)
		{
			m_imguiRenderer.Shutdown();
			m_imguiBackend.Shutdown();
			m_imguiInitialized = false;
		}

		delete m_pShaderManager;
		m_pShaderManager = nullptr;
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

		UpdateSample(dt);

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
		if (m_imguiInitialized) 
		{
			m_imguiBackend.OnKeyDown(keycode);
		}
	}

	void BaseSample::OnKeyUp(PHX::KeyCode keycode)
	{
		InputManager::GetInstance().SetKeyCode(keycode, false);
		if (m_imguiInitialized)
		{
			m_imguiBackend.OnKeyUp(keycode);
		}
	}

	void BaseSample::OnMouseButtonDown(PHX::MouseButtonCode mouseButton)
	{
		InputManager::GetInstance().SetMouseButton(mouseButton, true);
		if (m_imguiInitialized)
		{
			m_imguiBackend.OnMouseButtonDown(mouseButton);
		}
	}

	void BaseSample::OnMouseButtonUp(PHX::MouseButtonCode mouseButton)
	{
		InputManager::GetInstance().SetMouseButton(mouseButton, false);
		if (m_imguiInitialized)
		{
			m_imguiBackend.OnMouseButtonUp(mouseButton);
		}
	}

	void BaseSample::OnMouseMoved(float newX, float newY)
	{
		InputManager::GetInstance().SetMousePosition(newX, newY);
		if (m_imguiInitialized)
		{
			m_imguiBackend.OnMouseMoved(newX, newY);
		}
	}

	void BaseSample::OnMouseScroll(float scrollX, float scrollY)
	{
		InputManager::GetInstance().SetMouseScroll(scrollX, scrollY);
		if (m_imguiInitialized)
		{
			m_imguiBackend.OnMouseScroll(scrollX, scrollY);
		}
	}

	void BaseSample::GenerateRenderGraphVisualization(const char* name)
	{
		const u32 frameNumber = m_renderGraph.GetFrameNumber();
		const u32 nameLen = 256;
		char renderGraphVisName[nameLen];
		snprintf(renderGraphVisName, nameLen, "%s/render_graph_viz/%s_RG_%u.dot", CACHE_ROOT_DIR, name, frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}
}