#pragma once

#include "PHX/interface/window.h"

// Forward declarations
struct GLFWwindow;

namespace PHX
{
	class WindowWin64 : public IWindow
	{
	public:

		explicit WindowWin64(const WindowCreateInfo& createInfo);
		~WindowWin64();

		void* GetNativeHandle() const;

		void Update(float deltaTime) override;
		bool InFocus() const override;
		bool ShouldClose() const override;

		u32 GetCurrentWidth() const override;
		u32 GetCurrentHeight() const override;
		const char* GetName() const override;
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