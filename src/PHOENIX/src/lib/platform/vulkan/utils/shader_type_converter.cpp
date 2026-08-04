
#include "shader_type_converter.h"

#include "BSL/sanity.h"

using namespace BSL;

namespace PHX
{
	namespace SHADER_UTILS
	{
		VkShaderStageFlagBits ConvertShaderStage(SHADER_STAGE stage)
		{
			switch (stage)
			{
			case SHADER_STAGE::VERTEX:                  return VK_SHADER_STAGE_VERTEX_BIT;
			case SHADER_STAGE::GEOMETRY:                return VK_SHADER_STAGE_GEOMETRY_BIT;
			case SHADER_STAGE::FRAGMENT:                return VK_SHADER_STAGE_FRAGMENT_BIT;
			case SHADER_STAGE::COMPUTE:                 return VK_SHADER_STAGE_COMPUTE_BIT;
			case SHADER_STAGE::RAYGEN:                  return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			case SHADER_STAGE::INTERSECTION:            return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			case SHADER_STAGE::ANY_HIT:                 return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			case SHADER_STAGE::CLOSEST_HIT:             return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			case SHADER_STAGE::MISS:                    return VK_SHADER_STAGE_MISS_BIT_KHR;
			case SHADER_STAGE::CALLABLE:                return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			case SHADER_STAGE::TESSELLATION_CONTROL:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			case SHADER_STAGE::TESSELLATION_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			case SHADER_STAGE::MAX: 
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert shader stage to Vulkan shader stage!");
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
		}

		VkShaderStageFlags ConvertShaderStageFlags(ShaderStageFlags flags)
		{
			VkShaderStageFlags result = 0;
			if (flags & SHADER_STAGE_FLAG_VERTEX)                  result |= VK_SHADER_STAGE_VERTEX_BIT;
			if (flags & SHADER_STAGE_FLAG_GEOMETRY)                result |= VK_SHADER_STAGE_GEOMETRY_BIT;
			if (flags & SHADER_STAGE_FLAG_FRAGMENT)                result |= VK_SHADER_STAGE_FRAGMENT_BIT;
			if (flags & SHADER_STAGE_FLAG_COMPUTE)                 result |= VK_SHADER_STAGE_COMPUTE_BIT;
			if (flags & SHADER_STAGE_FLAG_RAYGEN)                  result |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_INTERSECTION)            result |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_ANY_HIT)                 result |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_CLOSEST_HIT)             result |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_MISS)                    result |= VK_SHADER_STAGE_MISS_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_CALLABLE)                result |= VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			if (flags & SHADER_STAGE_FLAG_TESSELLATION_CONTROL)    result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			if (flags & SHADER_STAGE_FLAG_TESSELLATION_EVALUATION) result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			return result;
		}
	}
}