#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/render_graph.h"

namespace PHX
{
	namespace RG_UTILS
	{
		VkPipelineBindPoint ConvertBindPoint(BIND_POINT bindPoint);
	}
}