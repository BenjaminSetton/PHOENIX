#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/shader_desc.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		VkShaderStageFlagBits ConvertShaderStage(SHADER_STAGE stage);
	}
}