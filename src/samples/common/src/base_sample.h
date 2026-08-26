#pragma once

#include <vector>

#include <PHX/phx.h>

#include "backends/imgui_backend_phx.h"
#include "backends/imgui_renderer_phx.h"
#include "camera/base_camera.h"
#include "input_manager.h"
#include "utils/shader_manager.h"

namespace Common
{
	// A single timed metric sample, used for time-windowed rolling averages
	struct MetricSample
	{
		float time  = 0.0f;  // accumulated time when the sample was recorded
		float value = 0.0f;  // value in ms
	};

	// Per-pass rolling statistics tracked across frames
	struct PassRollingStats
	{
		char passName[PHX::MAX_PASS_NAME_LEN] = {};
		std::vector<MetricSample> samples;
	};

	class BaseSample
	{
	public:

		BaseSample();
		virtual ~BaseSample();

		BaseSample(BaseSample&& other) = delete;
		BaseSample(const BaseSample& other) = delete;
		BaseSample& operator=(const BaseSample& other) = delete;

		void Init();
		void Shutdown();
		bool Update(float dt);
		virtual void Draw() = 0;

	protected:

		virtual void InitSample() = 0;
		virtual void ShutdownSample() = 0;
		virtual void UpdateSample(float dt) = 0;

		// Displays PHX metrics on an ImGui window
		virtual void ShowMetrics(float dt);

		virtual void OverrideSettings(PHX::Settings& settings);

		virtual void CreateWindow();
		virtual void CreateSwapChain();
		virtual void CreateRenderDevice();
		virtual void CreateRenderGraph();

		virtual void OnKeyDown(PHX::KeyCode keycode);
		virtual void OnKeyUp(PHX::KeyCode keycode);
		virtual void OnMouseButtonDown(PHX::MouseButtonCode mouseButton);
		virtual void OnMouseButtonUp(PHX::MouseButtonCode mouseButton);
		virtual void OnMouseMoved(float newX, float newY);
		virtual void OnMouseScroll(float scrollX, float scrollY);

		void GenerateRenderGraphVisualization(const char* name);

	protected:

		PHX::WindowHandle m_window;
		PHX::RenderDeviceHandle m_renderDevice;
		PHX::SwapChainHandle m_swapChain;
		PHX::RenderGraphHandle m_renderGraph;

		BaseCamera* m_pCamera;
		ShaderManager* m_pShaderManager;

		ImGuiPhxBackend  m_imguiBackend;
		ImGuiPhxRenderer m_imguiRenderer;

		// Rolling metrics state for temporally-stable display.
		// Samples are pruned to a METRICS_WINDOW_SECONDS time window each frame.
		static constexpr float METRICS_WINDOW_SECONDS = 1.0f;
		static constexpr u32   GPU_FRAME_TIME_HISTORY_COUNT = 120;
		// History plot updates at this interval (seconds) rather than every frame
		static constexpr float HISTORY_SAMPLE_INTERVAL = 1.0f;

		float m_metricsAccumulatedTime = 0.0f;
		float m_nextHistorySampleTime = 0.0f;
		std::vector<MetricSample> m_gpuFrameTimeSamples;
		std::vector<PassRollingStats> m_passRollingStats;

		// Ring buffer of rolling-average GPU frame times for the history plot
		float m_gpuFrameTimeHistory[GPU_FRAME_TIME_HISTORY_COUNT] = {};
		u32   m_gpuFrameTimeHistoryOffset = 0;

	private:

		bool m_imguiInitialized;
	};
}