
#include "attachment_type_converter.h"

namespace PHX
{
	namespace ATT_UTILS
	{
		VkAttachmentLoadOp ConvertLoadOp(ATTACHMENT_LOAD_OP loadOp)
		{
			switch (loadOp)
			{
			case ATTACHMENT_LOAD_OP::LOAD: return VK_ATTACHMENT_LOAD_OP_LOAD;
			case ATTACHMENT_LOAD_OP::CLEAR: return VK_ATTACHMENT_LOAD_OP_CLEAR;
			case ATTACHMENT_LOAD_OP::IGNORE: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

			case ATTACHMENT_LOAD_OP::INVALID: break;
			}

			return VK_ATTACHMENT_LOAD_OP_MAX_ENUM;
		}

		VkAttachmentStoreOp ConvertStoreOp(ATTACHMENT_STORE_OP storeOp)
		{
			switch (storeOp)
			{
			case ATTACHMENT_STORE_OP::STORE: return VK_ATTACHMENT_STORE_OP_STORE;
			case ATTACHMENT_STORE_OP::IGNORE: return VK_ATTACHMENT_STORE_OP_DONT_CARE;

			case ATTACHMENT_STORE_OP::INVALID: break;
			}

			return VK_ATTACHMENT_STORE_OP_NONE;
		}
	}
}