#pragma once

struct GLFWwindow;

#include "PHX/interface/window.h"

namespace PHX
{
	class WindowWin64 : public IWindow
	{
	public:

		explicit WindowWin64(const WindowCreateInfo& createInfo);
		~WindowWin64();

		void Update(float deltaTime) override;
		bool InFocus() override;
		bool ShouldClose() override;

		u32 GetCurrentWidth() override;
		u32 GetCurrentHeight() override;
		const char* GetName() override;
		fp_onWindowResized GetWindowResizedCallback() const;
		fp_onWindowFocusChanged GetWindowFocusChangedCallback() const;

		void SetWindowTitle(const char* format, ...) override;
		void SetInFocus(bool inFocus);

	private:

		u32 m_width;
		u32 m_height;
		const char* m_title;
		bool m_inFocus;
		fp_onWindowResized m_windowResizedCallback;
		fp_onWindowFocusChanged m_windowFocusChangedCallback;

		GLFWwindow* m_handle;

	};
}