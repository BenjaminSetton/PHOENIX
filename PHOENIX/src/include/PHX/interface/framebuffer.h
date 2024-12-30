#pragma once

#include "../types/attachment_desc.h"
#include "../types/integral_types.h"
#include "texture.h"

namespace PHX
{
	struct FramebufferAttachmentDesc
	{
		ITexture* pTexture          = nullptr;
		u32 mipTarget               = 0;
		ATTACHMENT_TYPE type        = ATTACHMENT_TYPE::INVALID;
		ATTACHMENT_LOAD_OP loadOp   = ATTACHMENT_LOAD_OP::INVALID;
		ATTACHMENT_STORE_OP storeOp = ATTACHMENT_STORE_OP::INVALID;
	};

	struct FramebufferCreateInfo
	{
		u32 width                               = 0;
		u32 height                              = 0;
		u32 layers                              = 0;
		FramebufferAttachmentDesc* pAttachments = nullptr;
		u32 attachmentCount                     = 0;
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