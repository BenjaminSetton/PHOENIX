#pragma once

#include <PHX/phx.h>

#include "camera/base_camera.h"
#include "input_manager.h"

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

		virtual bool Update(float dt);
		virtual void Draw() = 0;

	protected:

		virtual void Init();
		virtual void Shutdown();

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

	protected:

		PHX::WindowHandle m_window;
		PHX::RenderDeviceHandle m_renderDevice;
		PHX::SwapChainHandle m_swapChain;
		PHX::RenderGraphHandle m_renderGraph;

		BaseCamera* m_pCamera;
	};
}