
#include <vulkan/vk_enum_string_helper.h>

#include "render_graph_type_converter.h"

#include "BSL/logger.h"
#include "BSL/sanity.h"

using namespace BSL;

namespace PHX
{
	namespace RG_UTILS
	{
		VkPipelineBindPoint ConvertPassTypeToBindPoint(PASS_TYPE passType)
		{
			switch (passType)
			{
			case PASS_TYPE::GRAPHICS:     return VK_PIPELINE_BIND_POINT_GRAPHICS;
			case PASS_TYPE::COMPUTE:      return VK_PIPELINE_BIND_POINT_COMPUTE;
			case PASS_TYPE::RAY_TRACING:  return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
			case PASS_TYPE::TRANSFER: // No pipeline
			case PASS_TYPE::AS_BUILD: // No pipeline
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert pass type to VkPipelineBindPoint");
			return VK_PIPELINE_BIND_POINT_MAX_ENUM;
		}

		const char* PassTypeToString(PASS_TYPE passType)
		{
			switch (passType)
			{
			case PASS_TYPE::GRAPHICS:     return "GRAPHICS";
			case PASS_TYPE::COMPUTE:      return "COMPUTE";
			case PASS_TYPE::TRANSFER:     return "TRANSFER";
			case PASS_TYPE::RAY_TRACING:  return "RAY_TRACING";
			case PASS_TYPE::AS_BUILD:     return "AS_BUILD";
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert pass type to string");
			return "UNKNOWN";
		}

		// Strips a leading prefix from a string if present (used to make Vulkan enum names compact)
		std::string StripVkPrefix(const std::string& str, const std::string& prefix)
		{
			if (str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0)
			{
				return str.substr(prefix.size());
			}
			return str;
		}

		// Compact image layout string, e.g. "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL" -> "SHADER_READ_ONLY_OPTIMAL"
		std::string ShortImageLayout(VkImageLayout layout)
		{
			return StripVkPrefix(string_VkImageLayout(layout), "VK_IMAGE_LAYOUT_");
		}
	}
}