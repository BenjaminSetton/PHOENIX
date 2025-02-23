
#include "shader_type_converter.h"

#include "../../../utils/sanity.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		VkShaderStageFlagBits ConvertShaderStage(SHADER_STAGE stage)
		{
			switch (stage)
			{
			case SHADER_STAGE::VERTEX:   return VK_SHADER_STAGE_VERTEX_BIT;
			case SHADER_STAGE::GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
			case SHADER_STAGE::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
			case SHADER_STAGE::COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
			case SHADER_STAGE::MAX: 
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert shader stage to Vulkan shader stage!");
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}
	}
}