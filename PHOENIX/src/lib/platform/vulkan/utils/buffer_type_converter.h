#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/buffer_desc.h"

namespace PHX
{
	namespace BUFFER_UTILS
	{
		VkBufferUsageFlagBits ConvertBufferUsage(BUFFER_USAGE usage);
	}
}