#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

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
	};

	class ISwapChain
	{
	public:

		virtual ~ISwapChain() { }

		virtual ITexture* GetCurrentImage() const = 0;
		virtual u32 GetImageCount() const = 0;
		virtual u32 GetCurrentImageIndex() const = 0;
		virtual STATUS_CODE Present() = 0;
		virtual void Resize(u32 newWidth, u32 newHeight) = 0;
	};
}