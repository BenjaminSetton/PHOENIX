
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

		VkPipelineBindPoint ConvertBindPoint(BIND_POINT bindPoint)
		{
			switch (bindPoint)
			{
			case BIND_POINT::GRAPHICS:
			{
				return VK_PIPELINE_BIND_POINT_GRAPHICS;
			}
			case BIND_POINT::COMPUTE:
			{
				return VK_PIPELINE_BIND_POINT_COMPUTE;
			}
			case BIND_POINT::TRANSFER: // No valid conversion
			default:
			{
				break;
			}
			}

			LogError("Failed to convert bind point to VkPipelineBindPoint");
			return VK_PIPELINE_BIND_POINT_MAX_ENUM;
		}

		VkBlendFactor ConvertBlendFactor(BLEND_FACTOR factor)
		{
			switch (factor)
			{
			case BLEND_FACTOR::ZERO:                     return VK_BLEND_FACTOR_ZERO;
			case BLEND_FACTOR::ONE:                      return VK_BLEND_FACTOR_ONE;
			case BLEND_FACTOR::SRC_COLOR:                return VK_BLEND_FACTOR_SRC_COLOR;
			case BLEND_FACTOR::ONE_MINUS_SRC_COLOR:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			case BLEND_FACTOR::DST_COLOR:                return VK_BLEND_FACTOR_DST_COLOR;
			case BLEND_FACTOR::ONE_MINUS_DST_COLOR:      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			case BLEND_FACTOR::SRC_ALPHA:                return VK_BLEND_FACTOR_SRC_ALPHA;
			case BLEND_FACTOR::ONE_MINUS_SRC_ALPHA:      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			case BLEND_FACTOR::DST_ALPHA:                return VK_BLEND_FACTOR_DST_ALPHA;
			case BLEND_FACTOR::ONE_MINUS_DST_ALPHA:      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			case BLEND_FACTOR::CONSTANT_COLOR:           return VK_BLEND_FACTOR_CONSTANT_COLOR;
			case BLEND_FACTOR::ONE_MINUS_CONSTANT_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
			case BLEND_FACTOR::CONSTANT_ALPHA:           return VK_BLEND_FACTOR_CONSTANT_ALPHA;
			case BLEND_FACTOR::ONE_MINUS_CONSTANT_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
			case BLEND_FACTOR::SRC_ALPHA_SATURATE:       return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
			}

			LogError("Failed to convert blend factor to VkBlendFactor");
			return VK_BLEND_FACTOR_MAX_ENUM;
		}

		VkBlendOp ConvertBlendOp(BLEND_OP op)
		{
			switch (op)
			{
			case BLEND_OP::ADD:              return VK_BLEND_OP_ADD;
			case BLEND_OP::SUBTRACT:         return VK_BLEND_OP_SUBTRACT;
			case BLEND_OP::REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
			case BLEND_OP::MIN_VALUE:        return VK_BLEND_OP_MIN;
			case BLEND_OP::MAX_VALUE:        return VK_BLEND_OP_MAX;
			}

			LogError("Failed to convert blend op to VkBlendOp");
			return VK_BLEND_OP_MAX_ENUM;
		}

		VkColorComponentFlags ConvertColorComponentFlags(ColorComponentFlags flags)
		{
			VkColorComponentFlags vkFlags = 0;

			if (flags & COLOR_COMPONENT_FLAG_R) vkFlags |= VK_COLOR_COMPONENT_R_BIT;
			if (flags & COLOR_COMPONENT_FLAG_G) vkFlags |= VK_COLOR_COMPONENT_G_BIT;
			if (flags & COLOR_COMPONENT_FLAG_B) vkFlags |= VK_COLOR_COMPONENT_B_BIT;
			if (flags & COLOR_COMPONENT_FLAG_A) vkFlags |= VK_COLOR_COMPONENT_A_BIT;

			return vkFlags;
		}
	}
}