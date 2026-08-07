#pragma once

#include <PHX/phx.h>

#include "backends/imgui_backend_phx.h"
#include "backends/imgui_renderer_phx.h"
#include "camera/base_camera.h"
#include "input_manager.h"
#include "utils/shader_manager.h"

namespace Common
{
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

	private:

		bool m_imguiInitialized;
	};
}