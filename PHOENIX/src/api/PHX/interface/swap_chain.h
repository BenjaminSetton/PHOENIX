#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/texture.h"

#include "PHX/interface/ref.h" // TODO - Move to lib

namespace PHX
{
	struct SwapChainCreateInfo
	{
		u32 width                   = 1920;
		u32 height                  = 1080;
		bool enableVSync            = false;
	};

	struct SwapChainHandle : public Handle
	{
		DECLARE_HANDLE(SwapChainHandle)

		TextureHandle GetCurrentImage() const;
		u32 GetImageCount() const;
		u32 GetCurrentImageIndex() const;
		STATUS_CODE Present();
		void Resize(u32 newWidth, u32 newHeight);

		u32 GetWidth() const;
		u32 GetHeight() const;
	};

	// TODO - Move to lib
	class ISwapChain : public RefCounted
	{
	public:

		virtual ~ISwapChain() { }

		virtual TextureHandle GetCurrentImage() const = 0;
		virtual u32 GetImageCount() const = 0;
		virtual u32 GetCurrentImageIndex() const = 0;
		virtual STATUS_CODE Present() = 0;
		virtual void Resize(u32 newWidth, u32 newHeight) = 0;

		virtual u32 GetWidth() const = 0;
		virtual u32 GetHeight() const = 0;
	};
}