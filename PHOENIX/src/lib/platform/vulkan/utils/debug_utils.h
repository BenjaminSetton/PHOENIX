#pragma once

#include <vulkan/vulkan.h>

namespace PHX
{
	namespace DEBUG_UTILS
	{
		// Sets a debug name on a Vulkan object. The name will appear in validation layer
		// messages and debug tools like RenderDoc. Only effective when VK_EXT_debug_utils
		// is enabled (i.e. validation layers are active).
		void SetObjectName(VkDevice device, VkObjectType objectType, uint64_t objectHandle, const char* name);
	}
}
