
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
	static void OnWindowResized(GLFWwindow* window, i32 newWidth, i32 newHeight)
	{
		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		auto windowResizedCallback = userPointer->GetWindowResizedCallback();
		if (windowResizedCallback != nullptr)
		{
			windowResizedCallback(static_cast<u32>(newWidth), static_cast<u32>(newHeight));
		}
	}

	static void OnWindowFocusChanged(GLFWwindow* window, int isFocused)
	{
		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		auto windowFocusChangedCallback = userPointer->GetWindowFocusChangedCallback();

		// isFocused should only ever be GLFW_TRUE or GLFW_FALSE
		const bool bInFocus = static_cast<bool>(isFocused);
		if (windowFocusChangedCallback != nullptr)
		{
			windowFocusChangedCallback(bInFocus);
		}

		userPointer->SetInFocus(bInFocus);
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

	WindowWin64::WindowWin64(const WindowCreateInfo& createInfo) : m_handle(nullptr)
	{
		if (m_handle != nullptr)
		{
			LogError("Failed to initialize window! A window has already been created");
			return;
		}

		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		const char* titleUsed = createInfo.title;
		if (createInfo.title == nullptr)
		{
			// I don't believe GLFW accepts nullptr for the title
			titleUsed = "";
		}
		m_handle = glfwCreateWindow(createInfo.width, createInfo.height, titleUsed, nullptr, nullptr);
		glfwSetWindowUserPointer(m_handle, this);
		glfwSetInputMode(m_handle, GLFW_CURSOR, ConvertGLFWCursorTypeToInternal(createInfo.cursorType));
		glfwSetFramebufferSizeCallback(m_handle, OnWindowResized);
		glfwSetWindowFocusCallback(m_handle, OnWindowFocusChanged);

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

		m_width = createInfo.width;
		m_height = createInfo.height;
		m_title = titleUsed;
		m_inFocus = true;
		m_windowResizedCallback = createInfo.windowResizedCallback;
		m_windowFocusChangedCallback = createInfo.windowFocusChangedCallback;
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

	u32 WindowWin64::GetCurrentWidth() const
	{
		return m_width;
	}

	u32 WindowWin64::GetCurrentHeight() const
	{
		return m_height;
	}

	const char* WindowWin64::GetName() const
	{
		return m_title;
	}

	fp_onWindowResized WindowWin64::GetWindowResizedCallback() const
	{
		return m_windowResizedCallback;
	}

	fp_onWindowFocusChanged WindowWin64::GetWindowFocusChangedCallback() const
	{
		return m_windowFocusChangedCallback;
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

	void WindowWin64::SetInFocus(bool inFocus)
	{
		m_inFocus = inFocus;
	}
}