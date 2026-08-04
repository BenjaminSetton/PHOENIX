#pragma once

#include "BSL/integral_types.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/texture.h"

namespace PHX
{
	struct SwapChainCreateInfo
	{
		u32 width                   = 1920;
		u32 height                  = 1080;
		bool enableVSync            = false;
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