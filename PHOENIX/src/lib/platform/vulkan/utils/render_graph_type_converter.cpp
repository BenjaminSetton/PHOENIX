
#include <vulkan/vk_enum_string_helper.h>

#include "render_graph_type_converter.h"

#include "utils/logger.h"

namespace PHX
{
	namespace RG_UTILS
	{
		VkPipelineBindPoint ConvertBindPoint(BIND_POINT bindPoint)
		{
			switch (bindPoint)
			{
			case BIND_POINT::GRAPHICS: return VK_PIPELINE_BIND_POINT_GRAPHICS;
			case BIND_POINT::COMPUTE:  return VK_PIPELINE_BIND_POINT_COMPUTE;
			}

			LogError("Failed to convert bind point to VkPipelineBindPoint");
			return VK_PIPELINE_BIND_POINT_MAX_ENUM;
		}

		const char* BindPointToString(BIND_POINT bp)
		{
			switch (bp)
			{
			case BIND_POINT::GRAPHICS: return "GRAPHICS";
			case BIND_POINT::COMPUTE:  return "COMPUTE";
			case BIND_POINT::TRANSFER: return "TRANSFER";
			default:                   return "UNKNOWN";
			}
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