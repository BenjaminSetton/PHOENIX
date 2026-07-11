
#include "buffer_type_converter.h"

#include "utils/logger.h"

namespace PHX
{
	namespace BUFFER_UTILS
	{
		VkBufferUsageFlags ConvertBufferUsage(BUFFER_USAGE usage)
		{
			switch (usage)
			{
			case BUFFER_USAGE::UNIFORM_BUFFER:                     return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			case BUFFER_USAGE::STORAGE_BUFFER:                     return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			case BUFFER_USAGE::INDEX_BUFFER:                       return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			case BUFFER_USAGE::VERTEX_BUFFER:                      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			case BUFFER_USAGE::INDIRECT_BUFFER:                    return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			case BUFFER_USAGE::ACCELERATION_STRUCTURE:             return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			case BUFFER_USAGE::ACCELERATION_STRUCTURE_BUILD_INPUT: return VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			}

			LogError("Failed to convert buffer usage flag to VkBufferUsageFlags");
			return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
		}

		VkIndexType ConvertIndexType(INDEX_TYPE type)
		{
			switch (type)
			{
			case INDEX_TYPE::U16: return VK_INDEX_TYPE_UINT16;
			case INDEX_TYPE::U32: return VK_INDEX_TYPE_UINT32;
			}

			LogError("Failed to convert index type to VkIndexType");
			return VK_INDEX_TYPE_MAX_ENUM;
		}
	}
}