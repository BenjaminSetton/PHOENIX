#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/uniform_desc.h"

namespace PHX
{
	namespace UNIFORM_UTILS
	{
		VkDescriptorType ConvertUniformType(UNIFORM_TYPE type);
	}
}