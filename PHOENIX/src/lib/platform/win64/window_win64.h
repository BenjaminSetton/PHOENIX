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
		bool IsMinimized() const override;
		bool IsMaximized() const override;

		u32 GetCurrentWidth() const override;
		u32 GetCurrentHeight() const override;
		int GetPositionX() const override;
		int GetPositionY() const override;
		const char* GetName() const override;

		void OnWindowResizedCallback(u32 newWidth, u32 newHeight);
		void OnWindowMovedCallback(int newX, int newY);
		void OnWindowFocusChangedCallback(bool inFocus);
		void OnWindowMinimizedCallback(bool wasIconified);
		void OnWindowMaximizedCallback(bool wasMaximized);
		void OnWindowKeyEventCallback(int key, int scancode, int action);

		void OnMouseMovedCallback(float newX, float newY);
		void OnMouseButtonEvent(int button, int action);

		void SetWindowTitle(const char* format, ...) override;

	private:

		Vec2u m_size;
		Vec2u m_position;
		const char* m_title;
		bool m_inFocus;
		bool m_isMinimized;
		bool m_isMaximized;

		GLFWwindow* m_handle;
	};
}