
#include "buffer_type_converter.h"

#include "utils/logger.h"

namespace PHX
{
	namespace BUFFER_UTILS
	{
		VkBufferUsageFlagBits ConvertBufferUsage(BUFFER_USAGE usage)
		{
			switch (usage)
			{
			case BUFFER_USAGE::TRANSFER_SRC:    return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			case BUFFER_USAGE::TRANSFER_DST:    return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			case BUFFER_USAGE::UNIFORM_BUFFER:  return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case BUFFER_USAGE::STORAGE_BUFFER:  return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			case BUFFER_USAGE::INDEX_BUFFER:    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			case BUFFER_USAGE::VERTEX_BUFFER:   return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case BUFFER_USAGE::INDIRECT_BUFFER: return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			}

			LogError("Failed to convert buffer usage flag to VkBufferUsageFlagBits");
			return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		}
	}
}