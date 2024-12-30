#pragma once

#include "../types/integral_types.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;
	class IWindow;
	class ITexture;

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

		virtual ITexture* GetImage(u32 imageIndex) const = 0;
		virtual u32 GetImageCount() const = 0;

	};
}