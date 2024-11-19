#pragma once

#include "../types/basic_types.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;
	class IWindow;

	struct SwapChainCreateInfo
	{
		u32 width                   = 1920;
		u32 height                  = 1080;
		bool enableVSync            = false;
		IRenderDevice* renderDevice = nullptr;
		IWindow* window             = nullptr;
	};

	class ISwapChain
	{
	public:

		virtual ~ISwapChain() { }

	};
}