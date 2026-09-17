#pragma once

#include "BSL/integral_types.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/texture.h"

namespace PHX
{
	// Controls how presented images are handed to the display
	enum class PRESENT_MODE : u8
	{
		FIFO,         // Frames queue up and each is shown for at least one refresh. No tearing and is capped at refresh rate
		FIFO_RELAXED, // FIFO except late frames are shown immediately. Tearing occurs when below refresh rate
		MAILBOX,      // The newest frame replaces any queued ones. Offers the lowest latency without tearing
		IMMEDIATE,    // Frames are shown as soon as they're presented. Useful for profiling. ZOOOOOOOOOOOOOOOOOOOOOM...
	};

	struct SwapChainCreateInfo
	{
		u32 width                   = 1920;
		u32 height                  = 1080;
		PRESENT_MODE presentMode    = PRESENT_MODE::IMMEDIATE;
	};

	struct PHX_API SwapChainHandle : public Handle
	{
		DECLARE_PHX_HANDLE(SwapChainHandle);

		TextureHandle GetCurrentImage() const;
		u32 GetImageCount() const;
		u32 GetCurrentImageIndex() const;
		void Resize(u32 newWidth, u32 newHeight);

		u32 GetWidth() const;
		u32 GetHeight() const;
	};
}