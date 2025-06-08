
#include <stdio.h> // vsprintf_s
#include <stdarg.h> // va_start, va_end

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/glfw3.h> // GLFWwindow
#include <glfw/glfw3native.h>
#undef GLFW_EXPOSE_NATIVE_WIN32

// These macros are defined from GLFW somewhere and they interfere
// with std::numeric_limits<type>::min/max() from integral_types.h
#undef min
#undef max

#include "window_win64.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"

namespace PHX
{
	static void Global_OnWindowResized(GLFWwindow* window, i32 newWidth, i32 newHeight)
	{
		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowResizedCallback(static_cast<u32>(newWidth), static_cast<u32>(newHeight));
	}

	static void Global_OnWindowMoved(GLFWwindow* window, int newX, int newY)
	{
		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowMovedCallback(newX, newY);
	}

	static void Global_OnWindowFocusChanged(GLFWwindow* window, int isFocused)
	{
		// isFocused should only ever be GLFW_TRUE or GLFW_FALSE
		const bool bInFocus = static_cast<bool>(isFocused);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowFocusChangedCallback(bInFocus);
	}

	static void Global_OnWindowMinimized(GLFWwindow* window, int wasIconified)
	{
		// If iconified parameter is true, then window was minimized. If false, then
		// the window was restored
		const bool bWasIconified = static_cast<bool>(wasIconified);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowMinimizedCallback(bWasIconified);
	}

	static void Global_OnWindowMaximized(GLFWwindow* window, int wasMaximized)
	{
		// If wasMaximized parameter is true, then window was maximized. If false, then
		// the window was restored
		const bool bWasMaximized = static_cast<bool>(wasMaximized);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowMaximizedCallback(bWasMaximized);
	}

	static u32 ConvertGLFWCursorTypeToInternal(CURSOR_TYPE cursorType)
	{
		switch (cursorType)
		{
		case CURSOR_TYPE::SHOWN:
		{
			return GLFW_CURSOR_NORMAL;
		}
		case CURSOR_TYPE::HIDDEN:
		{
			return GLFW_CURSOR_HIDDEN;
		}
		case CURSOR_TYPE::DISABLED:
		{
			return GLFW_CURSOR_DISABLED;
		}
		}

		LogError("Invalid cursor type received! Defaulting to shown cursor");
		return GLFW_CURSOR_NORMAL;
	}

	WindowWin64::WindowWin64(const WindowCreateInfo& createInfo) : m_size(0), m_position(0), m_title(""), m_inFocus(true), 
																   m_isMinimized(false), m_isMaximized(false), m_handle(nullptr)
	{
		if (m_handle != nullptr)
		{
			LogError("Failed to initialize window! A window has already been created");
			return;
		}

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, createInfo.canResize ? GLFW_TRUE : GLFW_FALSE);

		const char* titleUsed = createInfo.title;
		if (createInfo.title == nullptr)
		{
			// I don't believe GLFW accepts nullptr for the title
			titleUsed = "";
		}
		m_handle = glfwCreateWindow(createInfo.size.GetX(), createInfo.size.GetY(), titleUsed, nullptr, nullptr);

		glfwSetWindowUserPointer(m_handle, this);
		glfwSetInputMode(m_handle, GLFW_CURSOR, ConvertGLFWCursorTypeToInternal(createInfo.cursorType));
		glfwSetFramebufferSizeCallback(m_handle, Global_OnWindowResized);
		glfwSetWindowPosCallback(m_handle, Global_OnWindowMoved);
		glfwSetWindowFocusCallback(m_handle, Global_OnWindowFocusChanged);
		glfwSetWindowIconifyCallback(m_handle, Global_OnWindowMinimized);
		glfwSetWindowMaximizeCallback(m_handle, Global_OnWindowMaximized);

		// Take the user's desired position and add it to the top-left coordinate to account for the menu bar
		int windowTop, windowLeft;
		glfwGetWindowFrameSize(m_handle, &windowLeft, &windowTop, nullptr, nullptr);
		glfwSetWindowPos(m_handle, createInfo.position.GetX() + windowLeft, createInfo.position.GetY() + windowTop);

		// Use raw mouse motion if available, meaning we won't consider any acceleration or other features that
		// are applied to a desktop mouse to make it "feel" better. Raw mouse motion is better for controlling 3D cameras
		if (glfwRawMouseMotionSupported())
		{
			glfwSetInputMode(m_handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		else
		{
			LogInfo("Raw mouse motion is not supported");
		}

		m_size = createInfo.size;
		m_position = createInfo.position;
		m_title = titleUsed;
		m_inFocus = true;
	}

	WindowWin64::~WindowWin64()
	{
		if (m_handle == nullptr)
		{
			LogError("Attempting to destroy window, but handle is already null?");
			return;
		}

		glfwDestroyWindow(m_handle);
		glfwTerminate();

		m_handle = nullptr;
	}

	void* WindowWin64::GetNativeHandle() const
	{
		return glfwGetWin32Window(m_handle);
	}

	void WindowWin64::Update(float deltaTime)
	{
		UNUSED(deltaTime);
		glfwPollEvents();
	}

	bool WindowWin64::InFocus() const
	{
		return m_inFocus;
	}

	bool WindowWin64::ShouldClose() const
	{
		return glfwWindowShouldClose(m_handle);
	}

	bool WindowWin64::IsMinimized() const
	{
		return m_isMinimized;
	}

	bool WindowWin64::IsMaximized() const
	{
		return m_isMaximized;
	}

	u32 WindowWin64::GetCurrentWidth() const
	{
		return m_size.GetX();
	}

	u32 WindowWin64::GetCurrentHeight() const
	{
		return m_size.GetY();
	}

	int WindowWin64::GetPositionX() const
	{
		return m_position.GetX();
	}

	int WindowWin64::GetPositionY() const
	{
		return m_position.GetY();
	}

	const char* WindowWin64::GetName() const
	{
		return m_title;
	}

	void WindowWin64::OnWindowResizedCallback(u32 newWidth, u32 newHeight)
	{
		if (m_isMinimized)
		{
			// Avoid changing the window dimensions if the resize was caused by a minimize
			return;
		}

		m_size = { newWidth, newHeight };

		auto& settings = GetSettings();
		if (settings.windowResizedCallback == nullptr)
		{
			LogError("Window was resized but no callback was provided for windowResizedCallback in Settings!");
			return;
		}

		settings.windowResizedCallback(m_size.GetX(), m_size.GetY());
	}

	void WindowWin64::OnWindowMovedCallback(int newX, int newY)
	{
		m_position = { static_cast<u32>(newX), static_cast<u32>(newY) };
	}

	void WindowWin64::OnWindowFocusChangedCallback(bool inFocus)
	{
		m_inFocus = inFocus;

		auto& settings = GetSettings();
		if (settings.windowFocusChangedCallback == nullptr)
		{
			LogError("Window focus was changed but no callback was provided for windowFocusChangedCallback in Settings!");
			return;
		}

		settings.windowFocusChangedCallback(m_inFocus);
	}

	void WindowWin64::OnWindowMinimizedCallback(bool wasMinimized)
	{
		m_isMinimized = wasMinimized;

		auto& settings = GetSettings();
		if (settings.windowMinimizedCallback == nullptr)
		{
			LogError("Window was minimized/restored but no callback was provided for windowMinimizedCallback in Settings!");
			return;
		}

		settings.windowMinimizedCallback(m_isMinimized);
	}

	void WindowWin64::OnWindowMaximizedCallback(bool wasMaximized)
	{
		m_isMaximized = wasMaximized;

		auto& settings = GetSettings();
		if (settings.windowMaximizedCallback == nullptr)
		{
			LogError("Window was maximized/restored but no callback was provided for windowMaximizedCallback in Settings!");
			return;
		}

		settings.windowMaximizedCallback(m_isMaximized);
	}

	void WindowWin64::SetWindowTitle(const char* format, ...)
	{
		static char buffer[100];
		va_list va;
		va_start(va, format);
		vsprintf_s(buffer, format, va);
		va_end(va);

		m_title = buffer;
		glfwSetWindowTitle(m_handle, m_title);
	}
}