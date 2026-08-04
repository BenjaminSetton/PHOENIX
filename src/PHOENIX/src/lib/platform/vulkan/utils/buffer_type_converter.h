#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/buffer_desc.h"

namespace PHX
{
	namespace BUFFER_UTILS
	{
		VkBufferUsageFlags ConvertBufferUsageFlags(BufferUsageFlags flags);
		VkIndexType ConvertIndexType(INDEX_TYPE type);
	}
}