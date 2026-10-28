#pragma once

#include <string>
#include <vulkan/vulkan.h>

#include "PHX/interface/render_graph.h"

namespace PHX
{
	namespace RG_UTILS
	{
		VkPipelineBindPoint ConvertPassTypeToBindPoint(PASS_TYPE passType);

		// Strips a leading prefix from a string if present (used to make Vulkan enum names compact)
		std::string StripVkPrefix(const std::string& str, const std::string& prefix);

		// Compact image layout string, e.g. "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL" -> "SHADER_READ_ONLY_OPTIMAL"
		std::string ShortImageLayout(VkImageLayout layout);
	}
}