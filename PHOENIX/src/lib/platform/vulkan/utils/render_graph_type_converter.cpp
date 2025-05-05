
#include "render_graph_type_converter.h"

#include "utils/logger.h"

namespace PHX
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
}