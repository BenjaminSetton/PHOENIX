#pragma once

#include <vulkan/vulkan.h>

#include <string>

#include "PHX/interface/render_graph.h"

namespace PHX
{
	namespace RG_UTILS
	{
		VkPipelineBindPoint ConvertBindPoint(BIND_POINT bindPoint);

		// TODO - Move to vulkan-specific render graph utils?

		const char* BindPointToString(BIND_POINT bp);

		// Strips a leading prefix from a string if present (used to make Vulkan enum names compact)
		std::string StripVkPrefix(const std::string& str, const std::string& prefix);

		// Compact image layout string, e.g. "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL" -> "SHADER_READ_ONLY_OPTIMAL"
		std::string ShortImageLayout(VkImageLayout layout);
	}
}