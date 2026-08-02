
#include "buffer_type_converter.h"

#include "utils/logger.h"

namespace PHX
{
	namespace BUFFER_UTILS
	{
		VkBufferUsageFlags ConvertBufferUsageFlags(BufferUsageFlags flags)
		{
			if (flags == BUFFER_USAGE_FLAG_INVALID)
			{
				LogError("Failed to convert buffer usage flags. Flags are INVALID (0)!");
				return VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
			}

			VkBufferUsageFlags res = 0;
			if (flags & BUFFER_USAGE_FLAG_UNIFORM_BUFFER)
			{
				res |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_STORAGE_BUFFER)
			{
				res |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_VERTEX_BUFFER)
			{
				res |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_INDEX_BUFFER)
			{
				res |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_INDIRECT_BUFFER)
			{
				res |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE)
			{
				res |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			}
			if (flags & BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT)
			{
				res |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			}

			return res;
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