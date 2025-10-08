#pragma once

#include <functional>

#include "PHX/types/integral_types.h"
#include "PHX/types/key_codes.h"
#include "PHX/types/mouse_codes.h"

namespace PHX
{
	// Forward declarations
	class ISwapChain;

	enum class GRAPHICS_API : u8
	{
		VULKAN = 0,
		//OPENGL, /*TODO*/
		//DX11,   /*TODO*/
		//DX12    /*TODO*/
	};

	enum class LOG_TYPE
	{
		DBG = -1,
		INFO,
		WARNING,
		ERR,
	};

	typedef std::function<void()>                                   fpSwapChainOutdatedCallback;
	typedef std::function<void(u32 newWidth, u32 newHeight)>        fpWindowResizedCallback;
	typedef std::function<void(bool inFocus)>                       fpWindowFocusChangedCallback;
	typedef std::function<void(bool wasMinimized)>                  fpWindowMinimizedCallback;
	typedef std::function<void(bool wasMaximized)>                  fpWindowMaximizedCallback;
	typedef std::function<void(KeyCode keycode)>                    fpWindowKeyEventCallback;
	typedef std::function<void(float newX, float newY)>             fpMouseMovedEventCallback;
	typedef std::function<void(MouseButtonCode keycode)>            fpMouseButtonEventCallback;
	typedef std::function<void(const char* msg, LOG_TYPE severity)> fpLogCallback;

	struct Settings
	{
		/* [MANDATORY] */ GRAPHICS_API backendAPI;                                             // Select the graphics backend API
		/* [MANDATORY] */ fpSwapChainOutdatedCallback swapChainOutdatedCallback     = nullptr; // Callback for when swap chain object became suboptimal or outdated
		/* [MANDATORY] */ fpWindowResizedCallback windowResizedCallback             = nullptr; // Callback for when the window is resized
		/* [MANDATORY] */ fpWindowFocusChangedCallback windowFocusChangedCallback   = nullptr; // Callback for when the window focus changes (e.g. minimize, maximize, clicking on other windows, etc)
		/* [MANDATORY] */ fpWindowMinimizedCallback windowMinimizedCallback         = nullptr; // Callback for when window is minimized (wasMinimized is true) or restored from a minimize (wasMinimized is false)
		/* [MANDATORY] */ fpWindowMaximizedCallback windowMaximizedCallback         = nullptr; // Callback for when window is maximized (wasMaximized is true) or restored from a maximize (wasMaximized is false)
		
		/* [OPTIONAL ] */ fpWindowKeyEventCallback windowKeyDownCallback            = nullptr; // Callback for when the window detects a key-press
		/* [OPTIONAL ] */ fpWindowKeyEventCallback windowKeyUpCallback              = nullptr; // Callback for when the window detects a key-press has been lifted
		/* [OPTIONAL ] */ fpWindowKeyEventCallback windowKeyRepeatCallback          = nullptr; // Callback for when the window detects a repeated key-press
		
		/* [OPTIONAL ] */ fpMouseMovedEventCallback mouseMovedCallback              = nullptr; // Callback for when the mouse moves over the window. Provides new mouse coordinates relative to the window's top-left corner
		/* [OPTIONAL ] */ fpMouseButtonEventCallback windowMouseButtonDownCallback  = nullptr; // Callback for when the window detects a mouse button press
		/* [OPTIONAL ] */ fpMouseButtonEventCallback windowMouseButtonUpCallback    = nullptr; // Callback for when the window detects a mouse button de-press

		/* [OPTIONAL ] */ fpLogCallback logCallback                                 = nullptr; // Provide a callback for log messages. If null, logs will use PHX's default log callback
		/* [OPTIONAL ] */ bool enableValidation                                     = false;   // Enable validation messages, whenever applicable
	};
}