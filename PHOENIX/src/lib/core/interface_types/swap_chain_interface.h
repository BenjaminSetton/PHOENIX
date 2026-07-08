#pragma once

#include "core/ref.h"
#include "PHX/interface/texture.h"
#include "PHX/types/status_code.h"

namespace PHX
{
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