#pragma once

#include "../types/integral_types.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;
	class ITexture;

	struct FramebufferCreateInfo
	{
		u32 width                    = 0;
		u32 height                   = 0;
		u32 layers                   = 0;
		ITexture** pAttachments      = nullptr;
		u32 attachmentCount          = 0;
		u32* pMipTargets             = nullptr;
		u32 mipTargetCount           = 0;
		IRenderDevice* pRenderDevice = nullptr;
	};

	class IFramebuffer
	{
	public:

		virtual ~IFramebuffer() { };

		virtual u32 GetWidth() = 0;
		virtual u32 GetHeight() = 0;
		virtual u32 GetLayers() = 0;
		virtual u32 GetAttachmentCount() = 0;
	};
}