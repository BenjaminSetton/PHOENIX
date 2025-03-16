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
	typedef void (*fpSwapChainRequiresResizeCallback)(ISwapChain* pSwapChain);

	struct Settings
	{
		GRAPHICS_API backendAPI;										// Select the graphics backend API
		bool enableValidation;											// Enable validation messages, whenever applicable
		fpSwapChainRequiresResizeCallback swapChainResizedCallback;		// Provide a callback for when the swap chain needs to be resized. This is done so that the new dimensions of the swap chain can be manually selected
		fpLogCallback logCallback;										// [OPTIONAL] Provide a callback for log messages. If null, logs will use PHX's default log callback
	};
}