#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/input_attribute.h"
#include "PHX/types/pipeline_desc.h"

namespace PHX
{
	namespace PIPELINE_UTILS
	{
		VkPrimitiveTopology ConvertPrimitiveTopology(PRIMITIVE_TOPOLOGY topology);
		VkPolygonMode ConvertPolygonMode(POLYGON_MODE polygonMode);
		VkCullModeFlagBits ConvertCullMode(CULL_MODE cullMode);
		VkFrontFace ConvertFrontFaceWinding(FRONT_FACE_WINDING winding);
		VkCompareOp ConvertCompareOp(COMPARE_OP compareOp);
		VkStencilOp ConvertStencilOp(STENCIL_OP stencilOp);
		VkVertexInputRate ConvertInputRate(VERTEX_INPUT_RATE inputRate);
	}
}