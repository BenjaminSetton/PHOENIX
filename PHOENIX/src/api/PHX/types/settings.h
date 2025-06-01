#pragma once

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Forward declarations
	class ISwapChain;

	enum class GRAPHICS_API : u8
	{
		VULKAN = 0,
		OPENGL,
		DX11,
		DX12
	};

	enum class LOG_TYPE
	{
		DBG = -1,
		INFO,
		WARNING,
		ERR,
	};

	typedef void (*fpLogCallback)(const char* msg, LOG_TYPE severity);
	typedef void (*fpSwapChainOutdatedCallback)();
	typedef void (*fpWindowResizedCallback)(u32 newWidth, u32 newHeight);
	typedef void (*fpWindowFocusChangedCallback)(bool inFocus);
	typedef void (*fpWindowMinimizedCallback)(bool wasMinimized);
	typedef void (*fpWindowMaximizedCallback)(bool wasMaximized);

	struct Settings
	{
		/* [MANDATORY] */ GRAPHICS_API backendAPI;												// Select the graphics backend API
		/* [OPTIONAL ] */ bool enableValidation										= false;	// Enable validation messages, whenever applicable
		/* [MANDATORY] */ fpSwapChainOutdatedCallback swapChainOutdatedCallback		= nullptr;	// Callback for when swap chain object became suboptimal or outdated
		/* [MANDATORY] */ fpWindowResizedCallback windowResizedCallback				= nullptr;	// Callback for when the window is resized
		/* [MANDATORY] */ fpWindowFocusChangedCallback windowFocusChangedCallback	= nullptr;	// Callback for when the window focus changes (e.g. minimize, maximize, clicking on other windows, etc)
		/* [MANDATORY] */ fpWindowMinimizedCallback windowMinimizedCallback			= nullptr;	// Callback for when window is minimized (wasMinimized is true) or restored from a minimize (wasMinimized is false)
		/* [MANDATORY] */ fpWindowMaximizedCallback windowMaximizedCallback			= nullptr;	// Callback for when window is maximized (wasMaximized is true) or restored from a maximize (wasMaximized is false)
		/* [OPTIONAL ] */ fpLogCallback logCallback									= nullptr;	// Provide a callback for log messages. If null, logs will use PHX's default log callback
	};
}