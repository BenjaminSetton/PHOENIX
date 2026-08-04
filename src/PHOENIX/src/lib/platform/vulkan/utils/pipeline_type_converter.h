#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/input_attribute.h"
#include "PHX/types/pipeline_desc.h"
#include "PHX/interface/render_graph.h"

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
		VkPipelineBindPoint ConvertPassType(PASS_TYPE passType);
		VkBlendFactor ConvertBlendFactor(BLEND_FACTOR factor);
		VkBlendOp ConvertBlendOp(BLEND_OP op);
		VkColorComponentFlags ConvertColorComponentFlags(ColorComponentFlags flags);
	}
}