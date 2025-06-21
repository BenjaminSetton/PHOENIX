
#include "uniform_type_converter.h"

#include "../../../utils/sanity.h"

namespace PHX
{
	namespace UNIFORM_UTILS
	{
		VkDescriptorType ConvertUniformType(UNIFORM_TYPE type)
		{
			switch (type)
			{
			case UNIFORM_TYPE::SAMPLER:					return VK_DESCRIPTOR_TYPE_SAMPLER;
			case UNIFORM_TYPE::COMBINED_IMAGE_SAMPLER:	return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case UNIFORM_TYPE::SAMPLED_IMAGE:			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			case UNIFORM_TYPE::STORAGE_IMAGE:			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			case UNIFORM_TYPE::UNIFORM_BUFFER:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case UNIFORM_TYPE::STORAGE_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			case UNIFORM_TYPE::INPUT_ATTACHMENT:		return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			case UNIFORM_TYPE::MAX:						break;
			}

			ASSERT_ALWAYS("Failed to convert PHX uniform type to Vulkan uniform type!");
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
		}
	}
}