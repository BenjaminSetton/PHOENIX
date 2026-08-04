#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/attachment_desc.h"

namespace PHX
{
	namespace ATT_UTILS
	{
		VkAttachmentLoadOp ConvertLoadOp(ATTACHMENT_LOAD_OP loadOp);
		VkAttachmentStoreOp ConvertStoreOp(ATTACHMENT_STORE_OP storeOp);
	}
}