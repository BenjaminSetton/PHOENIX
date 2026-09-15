
#include <algorithm>
#include <cstdio>
#include <cstring>
//#include <iostream>

#include "base_sample.h"
#include "BSL/logger.h"
#include "input_manager.h"

#ifndef CACHE_ROOT_DIR
	#define CACHE_ROOT_DIR "."
#endif

#define CHECK_PHX_RES(phxRes) if(phxRes != PHX::STATUS_CODE::SUCCESS) { return; }

using namespace BSL;
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
		settings.backendAPIMinorVersion = 1;
		settings.logCallback = nullptr;

		settings.enableValidation = false; // TODO - Add DEBUG project define for samples and guard this setting based on that
		settings.swapChainOutdatedCallback = OnSwapChainOutdatedCallback;
		settings.windowFocusChangedCallback = OnWindowFocusChangedCallback;
		settings.windowMaximizedCallback = OnWindowMaximizedCallback;
		settings.windowMinimizedCallback = OnWindowMinimizedCallback;
		settings.windowResizedCallback = OnWindowResizedCallback;
		settings.gatherMetrics = true;
		settings.cacheDirectory = CACHE_ROOT_DIR;

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
			LogError("Failed to initialize ImGui backend!");;
			return;
		}
		if (!m_imguiRenderer.Init(m_renderDevice, m_swapChain, m_pShaderManager))
		{
			LogError("Failed to initialize ImGui renderer!");;
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
			LogError("Failed to clean up PHX lib!");
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
			LogError("Failed to update PHX lib!");
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

		// Begin new ImGui frame, if applicable
		if (m_imguiInitialized)
		{
			m_imguiBackend.NewFrame(dt, m_swapChain.GetWidth(), m_swapChain.GetHeight());
		}

		UpdateSample(dt);

		ShowMetrics(dt);

		// Render ImGui, if applicable
		if (m_imguiInitialized)
		{
			ImGui::Render();
		}

		return m_window.ShouldClose();
	}

	void BaseSample::ShowMetrics(float dt)
	{
		m_metricsAccumulatedTime += dt;
		const float windowStart = m_metricsAccumulatedTime - METRICS_WINDOW_SECONDS;

		const PHX::Metrics& metrics = m_renderGraph.GetMetrics();

		// --- GPU frame time rolling stats ---
		// Only add non-zero samples; zero means queries were unavailable this frame
		if (metrics.gpuFrameTime > 0.0f)
		{
			m_gpuFrameTimeSamples.push_back({m_metricsAccumulatedTime, metrics.gpuFrameTime});
		}

		// Prune samples outside the time window
		m_gpuFrameTimeSamples.erase(
			std::remove_if(m_gpuFrameTimeSamples.begin(), m_gpuFrameTimeSamples.end(),
				[windowStart](const MetricSample& s) { return s.time < windowStart; }),
			m_gpuFrameTimeSamples.end());

		float gpuAvg = 0.0f, gpuDev = 0.0f;
		if (!m_gpuFrameTimeSamples.empty())
		{
			float sum = 0.0f, minVal = m_gpuFrameTimeSamples[0].value, maxVal = m_gpuFrameTimeSamples[0].value;
			for (const MetricSample& s : m_gpuFrameTimeSamples)
			{
				sum += s.value;
				minVal = std::min(minVal, s.value);
				maxVal = std::max(maxVal, s.value);
			}
			gpuAvg = sum / static_cast<float>(m_gpuFrameTimeSamples.size());
			gpuDev = maxVal - minVal;
		}

		ImGui::Text("CPU dt: %.3f ms", dt * 1000.0f);
		ImGui::Text("GPU frame time: %.3f ms. Average %.3f (+- %.3f) ms", metrics.gpuFrameTime, gpuAvg, gpuDev);

		// History plot stores rolling averages, sampled at a fixed interval (not every frame)
		if (m_metricsAccumulatedTime >= m_nextHistorySampleTime)
		{
			m_gpuFrameTimeHistory[m_gpuFrameTimeHistoryOffset] = gpuAvg;
			m_gpuFrameTimeHistoryOffset = (m_gpuFrameTimeHistoryOffset + 1) % GPU_FRAME_TIME_HISTORY_COUNT;
			m_nextHistorySampleTime = m_metricsAccumulatedTime + HISTORY_SAMPLE_INTERVAL;
		}

		float historyMax = 0.0f;
		for (float sample : m_gpuFrameTimeHistory)
		{
			historyMax = std::max(historyMax, sample);
		}

		char overlayText[32];
		snprintf(overlayText, sizeof(overlayText), "%.3f ms", gpuAvg);
		ImGui::PlotLines("GPU frame time history (rolling avg)", m_gpuFrameTimeHistory, static_cast<int>(GPU_FRAME_TIME_HISTORY_COUNT),
			static_cast<int>(m_gpuFrameTimeHistoryOffset), overlayText, 0.0f, historyMax * 1.1f, ImVec2(0.0f, 80.0f));

		// --- Per-pass rolling stats ---
		// Update rolling samples for each pass reported this frame
		for (const PHX::PassTiming& timing : metrics.passTimings)
		{
			if (timing.timeInMs <= 0.0f) { continue; }

			// Find or create rolling stats entry by pass name
			PassRollingStats* pStats = nullptr;
			for (PassRollingStats& stats : m_passRollingStats)
			{
				if (std::strcmp(stats.passName, timing.passName) == 0)
				{
					pStats = &stats;
					break;
				}
			}
			if (pStats == nullptr)
			{
				m_passRollingStats.emplace_back();
				pStats = &m_passRollingStats.back();
				std::memcpy(pStats->passName, timing.passName, PHX::MAX_PASS_NAME_LEN);
			}

			pStats->samples.push_back({m_metricsAccumulatedTime, timing.timeInMs});

			// Prune old samples
			pStats->samples.erase(
				std::remove_if(pStats->samples.begin(), pStats->samples.end(),
					[windowStart](const MetricSample& s) { return s.time < windowStart; }),
				pStats->samples.end());
		}

		// Display per-pass timings with rolling averages
		if (ImGui::CollapsingHeader("GPU Timings"))
		{
			for (const PassRollingStats& stats : m_passRollingStats)
			{
				if (stats.samples.empty()) { continue; }

				float sum = 0.0f, minVal = stats.samples[0].value, maxVal = stats.samples[0].value;
				for (const MetricSample& s : stats.samples)
				{
					sum += s.value;
					minVal = std::min(minVal, s.value);
					maxVal = std::max(maxVal, s.value);
				}
				const float avg = sum / static_cast<float>(stats.samples.size());
				const float dev = maxVal - minVal;
				ImGui::Text("\t%s: %.3f (+- %.3f) ms", stats.passName, avg, dev);
			}
		}
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
		windowCI.windowMode = WINDOW_MODE::WINDOWED;
		windowCI.canResize = false;
		windowCI.size = { 1920, 1080 };
		windowCI.position = { 400, 400 };

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
		renderDeviceCI.framesInFlight = 3;
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
		snprintf(renderGraphVisName, nameLen, "render_graph_viz/%s_RG_%u.dot", name, frameNumber);
		m_renderGraph.GenerateVisualization(renderGraphVisName);
	}

	void BaseSample::RenderImGui(bool clearBackbuffer)
	{
		if (!m_imguiInitialized)
		{
			return;
		}

		m_imguiRenderer.RenderDrawData(m_renderGraph, m_swapChain, ImGui::GetDrawData(), clearBackbuffer);
	}
}