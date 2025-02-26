
#include "pipeline_type_converter.h"

#include "utils/logger.h"

namespace PHX
{
	namespace PIPELINE_UTILS
	{
		VkPrimitiveTopology ConvertPrimitiveTopology(PRIMITIVE_TOPOLOGY topology)
		{
			switch (topology)
			{
			case PRIMITIVE_TOPOLOGY::POINT_LIST:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			case PRIMITIVE_TOPOLOGY::LINE_LIST:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			case PRIMITIVE_TOPOLOGY::LINE_STRIP:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			case PRIMITIVE_TOPOLOGY::TRIANGLE_LIST:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			case PRIMITIVE_TOPOLOGY::TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			case PRIMITIVE_TOPOLOGY::TRIANGLE_FAN:   return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
			}

			LogError("Failed to convert primitive topology to VkPrimitiveTopology");
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
		}

		VkPolygonMode ConvertPolygonMode(POLYGON_MODE polygonMode)
		{
			switch (polygonMode)
			{
			case POLYGON_MODE::FILL:  return VK_POLYGON_MODE_FILL;
			case POLYGON_MODE::LINE:  return VK_POLYGON_MODE_LINE;
			case POLYGON_MODE::POINT: return VK_POLYGON_MODE_POINT;
			}

			LogError("Failed to convert polygon mode to VkPolygonMode");
			return VK_POLYGON_MODE_MAX_ENUM;
		}

		VkCullModeFlagBits ConvertCullMode(CULL_MODE cullMode)
		{
			switch (cullMode)
			{
			case CULL_MODE::NONE:           return VK_CULL_MODE_NONE;
			case CULL_MODE::FRONT:          return VK_CULL_MODE_FRONT_BIT;
			case CULL_MODE::BACK:           return VK_CULL_MODE_BACK_BIT;
			case CULL_MODE::FRONT_AND_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
			}

			LogError("Failed to convert cull mode to VkCullModeFlagBits");
			return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
		}

		VkFrontFace ConvertFrontFaceWinding(FRONT_FACE_WINDING winding)
		{
			switch (winding)
			{
			case FRONT_FACE_WINDING::COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
			case FRONT_FACE_WINDING::CLOCKWISE:         return VK_FRONT_FACE_CLOCKWISE;
			}

			LogError("Failed to convert front face winding to VkFrontFace");
			return VK_FRONT_FACE_MAX_ENUM;
		}

		VkCompareOp ConvertCompareOp(COMPARE_OP compareOp)
		{
			switch (compareOp)
			{
			case COMPARE_OP::NEVER:            return VK_COMPARE_OP_NEVER;
			case COMPARE_OP::LESS:             return VK_COMPARE_OP_LESS;
			case COMPARE_OP::EQUAL:            return VK_COMPARE_OP_EQUAL;
			case COMPARE_OP::LESS_OR_EQUAL:    return VK_COMPARE_OP_LESS_OR_EQUAL;
			case COMPARE_OP::GREATER:          return VK_COMPARE_OP_GREATER;
			case COMPARE_OP::NOT_EQUAL:        return VK_COMPARE_OP_NOT_EQUAL;
			case COMPARE_OP::GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
			case COMPARE_OP::ALWAYS:           return VK_COMPARE_OP_ALWAYS;
			}

			LogError("Failed to convert compare op to VkCompareOp");
			return VK_COMPARE_OP_MAX_ENUM;
		}

		VkStencilOp ConvertStencilOp(STENCIL_OP stencilOp)
		{
			switch (stencilOp)
			{
			case STENCIL_OP::KEEP:                return VK_STENCIL_OP_KEEP;
			case STENCIL_OP::ZERO:                return VK_STENCIL_OP_ZERO;
			case STENCIL_OP::REPLACE:             return VK_STENCIL_OP_REPLACE;
			case STENCIL_OP::INCREMENT_AND_CLAMP: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
			case STENCIL_OP::DECREMENT_AND_CLAMP: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
			case STENCIL_OP::INVERT:              return VK_STENCIL_OP_INVERT;
			case STENCIL_OP::INCREMENT_AND_WRAP:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
			case STENCIL_OP::DECREMENT_AND_WRAP:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
			}

			LogError("Failed to convert stencil op to VkStencilOp");
			return VK_STENCIL_OP_MAX_ENUM;
		}

		VkVertexInputRate ConvertInputRate(VERTEX_INPUT_RATE inputRate)
		{
			switch (inputRate)
			{
			case VERTEX_INPUT_RATE::PER_VERTEX:
			{
				return VK_VERTEX_INPUT_RATE_VERTEX;
			}
			case VERTEX_INPUT_RATE::PER_INSTANCE:
			{
				return VK_VERTEX_INPUT_RATE_INSTANCE;
			}
			}

			LogError("Failed to convert attribute input rate to VkVertexInputRate");
			return VK_VERTEX_INPUT_RATE_MAX_ENUM;
		}
	}
}