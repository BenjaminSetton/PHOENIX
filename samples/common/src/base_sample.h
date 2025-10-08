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

		virtual void CreateWindow();
		virtual void CreateSwapChain();
		virtual void CreateRenderDevice();
		virtual void CreateRenderGraph();

		virtual void DestroyWindow();
		virtual void DestroySwapChain();
		virtual void DestroyRenderDevice();
		virtual void DestroyRenderGraph();

	protected:

		PHX::IWindow* m_pWindow;
		PHX::ISwapChain* m_pSwapChain;
		PHX::IRenderDevice* m_pRenderDevice;
		PHX::IRenderGraph* m_pRenderGraph;

		BaseCamera* m_pCamera;

	private:

		void OnKeyDown(PHX::KeyCode keycode);
		void OnKeyUp(PHX::KeyCode keycode);
		void OnMouseButtonDown(PHX::MouseButtonCode mouseButton);
		void OnMouseButtonUp(PHX::MouseButtonCode mouseButton);
		void OnMouseMoved(float newX, float newY);
	};
}