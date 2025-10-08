
#include <stdio.h> // vsprintf_s
#include <stdarg.h> // va_start, va_end
#include <unordered_map>

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
#include "PHX/types/key_codes.h"
#include "PHX/types/mouse_codes.h"

namespace PHX
{
	static constexpr u32 MAX_WINDOW_TITLE_SIZE = 256;

	static const std::unordered_map<int, KeyCode> s_KeyCodeMappings =
	{
		// Numbers
		{ GLFW_KEY_0, KeyCode::KEY_0 },
		{ GLFW_KEY_1, KeyCode::KEY_1 },
		{ GLFW_KEY_2, KeyCode::KEY_2 },
		{ GLFW_KEY_3, KeyCode::KEY_3 },
		{ GLFW_KEY_4, KeyCode::KEY_4 },
		{ GLFW_KEY_5, KeyCode::KEY_5 },
		{ GLFW_KEY_6, KeyCode::KEY_6 },
		{ GLFW_KEY_7, KeyCode::KEY_7 },
		{ GLFW_KEY_8, KeyCode::KEY_8 },
		{ GLFW_KEY_9, KeyCode::KEY_9 },

		// Letters
		{ GLFW_KEY_A, KeyCode::KEY_A },
		{ GLFW_KEY_B, KeyCode::KEY_B },
		{ GLFW_KEY_C, KeyCode::KEY_C },
		{ GLFW_KEY_D, KeyCode::KEY_D },
		{ GLFW_KEY_E, KeyCode::KEY_E },
		{ GLFW_KEY_F, KeyCode::KEY_F },
		{ GLFW_KEY_G, KeyCode::KEY_G },
		{ GLFW_KEY_H, KeyCode::KEY_H },
		{ GLFW_KEY_I, KeyCode::KEY_I },
		{ GLFW_KEY_J, KeyCode::KEY_J },
		{ GLFW_KEY_K, KeyCode::KEY_K },
		{ GLFW_KEY_L, KeyCode::KEY_L },
		{ GLFW_KEY_M, KeyCode::KEY_M },
		{ GLFW_KEY_N, KeyCode::KEY_N },
		{ GLFW_KEY_O, KeyCode::KEY_O },
		{ GLFW_KEY_P, KeyCode::KEY_P },
		{ GLFW_KEY_Q, KeyCode::KEY_Q },
		{ GLFW_KEY_R, KeyCode::KEY_R },
		{ GLFW_KEY_S, KeyCode::KEY_S },
		{ GLFW_KEY_T, KeyCode::KEY_T },
		{ GLFW_KEY_U, KeyCode::KEY_U },
		{ GLFW_KEY_V, KeyCode::KEY_V },
		{ GLFW_KEY_W, KeyCode::KEY_W },
		{ GLFW_KEY_X, KeyCode::KEY_X },
		{ GLFW_KEY_Y, KeyCode::KEY_Y },
		{ GLFW_KEY_Z, KeyCode::KEY_Z },

		// Utility keys
		//	KEY_OSKEY,
		{ GLFW_KEY_LEFT_SHIFT   , KeyCode::KEY_LSHIFT      },
		{ GLFW_KEY_RIGHT_SHIFT  , KeyCode::KEY_RSHIFT      },
		{ GLFW_KEY_LEFT_CONTROL , KeyCode::KEY_LCTRL       },
		{ GLFW_KEY_RIGHT_CONTROL, KeyCode::KEY_RCTRL       },
		{ GLFW_KEY_SPACE        , KeyCode::KEY_SPACEBAR    },
		{ GLFW_KEY_LEFT_ALT     , KeyCode::KEY_LALT        },
		{ GLFW_KEY_RIGHT_ALT    , KeyCode::KEY_RALT        },
		{ GLFW_KEY_TAB          , KeyCode::KEY_TAB         },
		{ GLFW_KEY_CAPS_LOCK    , KeyCode::KEY_CAPS        },
		{ GLFW_KEY_ENTER        , KeyCode::KEY_ENTER       },
		{ GLFW_KEY_BACKSPACE    , KeyCode::KEY_BACKSPACE   },
		{ GLFW_KEY_ESCAPE       , KeyCode::KEY_ESC         },
		{ GLFW_KEY_HOME         , KeyCode::KEY_HOME        },
		{ GLFW_KEY_END          , KeyCode::KEY_END         },
		{ GLFW_KEY_INSERT       , KeyCode::KEY_INSERT      },
		{ GLFW_KEY_DELETE       , KeyCode::KEY_DELETE      },
		{ GLFW_KEY_PAGE_UP      , KeyCode::KEY_PGUP        },
		{ GLFW_KEY_PAGE_DOWN    , KeyCode::KEY_PGDOWN      },
		{ GLFW_KEY_LEFT         , KeyCode::KEY_LEFT_ARROW  },
		{ GLFW_KEY_RIGHT        , KeyCode::KEY_RIGHT_ARROW },
		{ GLFW_KEY_UP           , KeyCode::KEY_UP_ARROW    },
		{ GLFW_KEY_DOWN         , KeyCode::KEY_DOWN_ARROW  },

		// Special characters
		//	KEY_TILDA,
		//	KEY_HYPHEN,
		{ GLFW_KEY_EQUAL        , KeyCode::KEY_EQUALS         },
		{ GLFW_KEY_LEFT_BRACKET , KeyCode::KEY_LSQUAREBRACKET },
		{ GLFW_KEY_RIGHT_BRACKET, KeyCode::KEY_RSQUAREBRACKET },
		{ GLFW_KEY_BACKSLASH    , KeyCode::KEY_BACKSLASH      },
		{ GLFW_KEY_SLASH        , KeyCode::KEY_FORWARDSLASH   },
		{ GLFW_KEY_SEMICOLON    , KeyCode::KEY_SEMICOLON      },
		{ GLFW_KEY_APOSTROPHE   , KeyCode::KEY_APOSTROPHE     },
		{ GLFW_KEY_COMMA        , KeyCode::KEY_COMMA          },
		{ GLFW_KEY_PERIOD       , KeyCode::KEY_PERIOD         },

		// Function keys
		{ GLFW_KEY_F1 , KeyCode::KEY_F1  },
		{ GLFW_KEY_F2 , KeyCode::KEY_F2  },
		{ GLFW_KEY_F3 , KeyCode::KEY_F3  },
		{ GLFW_KEY_F4 , KeyCode::KEY_F4  },
		{ GLFW_KEY_F5 , KeyCode::KEY_F5  },
		{ GLFW_KEY_F6 , KeyCode::KEY_F6  },
		{ GLFW_KEY_F7 , KeyCode::KEY_F7  },
		{ GLFW_KEY_F8 , KeyCode::KEY_F8  },
		{ GLFW_KEY_F9 , KeyCode::KEY_F9  },
		{ GLFW_KEY_F10, KeyCode::KEY_F10 },
		{ GLFW_KEY_F11, KeyCode::KEY_F11 },
		{ GLFW_KEY_F12, KeyCode::KEY_F12 },

	};

	static const std::unordered_map<int, MouseButtonCode> s_MouseButtonCodeMappings =
	{
		{ GLFW_MOUSE_BUTTON_LEFT  , MouseButtonCode::MOUSE_LMB     },
		{ GLFW_MOUSE_BUTTON_MIDDLE, MouseButtonCode::MOUSE_MMB     },
		{ GLFW_MOUSE_BUTTON_RIGHT , MouseButtonCode::MOUSE_RMB     },
		{ GLFW_MOUSE_BUTTON_1     , MouseButtonCode::MOUSE_BUTTON1 },
		{ GLFW_MOUSE_BUTTON_2     , MouseButtonCode::MOUSE_BUTTON2 },
		{ GLFW_MOUSE_BUTTON_3     , MouseButtonCode::MOUSE_BUTTON3 },
	};

	static void Global_OnWindowResized(GLFWwindow* window, int newWidth, int newHeight)
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

	static void Global_OnKeyEvent(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		UNUSED(mods);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnWindowKeyEventCallback(key, scancode, action);
	}

	static void Global_OnMouseMoved(GLFWwindow* window, double xpos, double ypos)
	{
		const float newX = static_cast<float>(xpos);
		const float newY = static_cast<float>(ypos);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnMouseMovedCallback(newX, newY);
	}

	static void Global_OnMouseButtonEvent(GLFWwindow* window, int button, int action, int mods)
	{
		UNUSED(mods);

		WindowWin64* userPointer = reinterpret_cast<WindowWin64*>(glfwGetWindowUserPointer(window));
		userPointer->OnMouseButtonEvent(button, action);
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
		glfwSetCursorPosCallback(m_handle, Global_OnMouseMoved);
		glfwSetMouseButtonCallback(m_handle, Global_OnMouseButtonEvent);
		glfwSetKeyCallback(m_handle, Global_OnKeyEvent);

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

	void WindowWin64::OnWindowKeyEventCallback(int key, int scancode, int action)
	{
		UNUSED(scancode);

		auto& settings = GetSettings();

		auto iter = s_KeyCodeMappings.find(key);
		if (iter == s_KeyCodeMappings.end())
		{
			ASSERT_ALWAYS("Failed to convert from GLFW scancode to PHX::KeyCode!");
			return;
		}

		KeyCode convertedKeyCode = iter->second;
		switch (action)
		{
		case GLFW_RELEASE:
		{
			if (settings.windowKeyUpCallback != nullptr)
			{
				settings.windowKeyUpCallback(convertedKeyCode);
			}
			break;
		}
		case GLFW_PRESS:
		{
			if (settings.windowKeyDownCallback != nullptr)
			{
				settings.windowKeyDownCallback(convertedKeyCode);
			}
			break;
		}
		case GLFW_REPEAT:
		{
			if (settings.windowKeyRepeatCallback != nullptr)
			{
				settings.windowKeyRepeatCallback(convertedKeyCode);
			}
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Received window key event with an unsupported action");
		}
		}
	}

	void WindowWin64::OnMouseMovedCallback(float newX, float newY)
	{
		auto& settings = GetSettings();
		if (settings.mouseMovedCallback != nullptr)
		{
			settings.mouseMovedCallback(newX, newY);
		}
	}

	void WindowWin64::OnMouseButtonEvent(int button, int action)
	{
		auto& settings = GetSettings();

		auto iter = s_MouseButtonCodeMappings.find(button);
		if (iter == s_MouseButtonCodeMappings.end())
		{
			ASSERT_ALWAYS("Failed to convert from GLFW button to PHX::MouseButtonCode!");
			return;
		}

		const MouseButtonCode mouseButton = iter->second;
		switch (action)
		{
		case GLFW_RELEASE:
		{
			if (settings.windowMouseButtonUpCallback != nullptr)
			{
				settings.windowMouseButtonUpCallback(mouseButton);
			}
			break;
		}
		case GLFW_PRESS:
		{
			if (settings.windowMouseButtonDownCallback != nullptr)
			{
				settings.windowMouseButtonDownCallback(mouseButton);
			}
			break;
		}
		case GLFW_REPEAT:
		{
			// Ignoring for now
			break;
		}
		default:
		{
			break;
		}
		}
	}

	void WindowWin64::SetWindowTitle(const char* format, ...)
	{
		static char buffer[MAX_WINDOW_TITLE_SIZE];
		va_list va;
		va_start(va, format);
		vsprintf_s(buffer, format, va);
		va_end(va);

		m_title = buffer;
		glfwSetWindowTitle(m_handle, m_title);
	}
}