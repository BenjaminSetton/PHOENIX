
#include "texture_type_converter.h"

#include "../../../utils/sanity.h"

namespace PHX
{
	STATIC_ASSERT_MSG(ASPECT_TYPE_MAX == 3u, "New enum entries must be included in TexUtils::ConvertAspectFlags()");
	STATIC_ASSERT_MSG(USAGE_TYPE_MAX == 8u, "New enum entries must be included in TexUtils::ConvertUsageFlags()");

	namespace TEX_UTILS
	{
		template<typename T, typename U> 
		static bool IsFlagSet(const U& bits, T flag)
		{
			return (bits & static_cast<U>(flag)) != 0;
		}

		VkFormat ConvertBaseFormat(BASE_FORMAT format)
		{
			switch (format)
			{
			case BASE_FORMAT::R8_UNORM:				return VK_FORMAT_R8_UNORM;
			case BASE_FORMAT::R8_SNORM:				return VK_FORMAT_R8_SNORM;
			case BASE_FORMAT::R8_UINT:				return VK_FORMAT_R8_UINT;
			case BASE_FORMAT::R8_SINT:				return VK_FORMAT_R8_SINT;
			case BASE_FORMAT::R8_SRGB:				return VK_FORMAT_R8_SRGB;
			case BASE_FORMAT::R8G8_UNORM:			return VK_FORMAT_R8G8_UNORM;
			case BASE_FORMAT::R8G8_SNORM:			return VK_FORMAT_R8G8_SNORM;
			case BASE_FORMAT::R8G8_UINT:			return VK_FORMAT_R8G8_UINT;
			case BASE_FORMAT::R8G8_SINT:			return VK_FORMAT_R8G8_SINT;
			case BASE_FORMAT::R8G8_SRGB:			return VK_FORMAT_R8G8_SRGB;
			case BASE_FORMAT::R8G8B8_UNORM:			return VK_FORMAT_R8G8B8_UNORM;
			case BASE_FORMAT::R8G8B8_SNORM:			return VK_FORMAT_R8G8B8_SNORM;
			case BASE_FORMAT::R8G8B8_UINT:			return VK_FORMAT_R8G8B8_UINT;
			case BASE_FORMAT::R8G8B8_SINT:			return VK_FORMAT_R8G8B8_SINT;
			case BASE_FORMAT::R8G8B8_SRGB:			return VK_FORMAT_R8G8B8_SRGB;
			case BASE_FORMAT::R8G8B8A8_UNORM:		return VK_FORMAT_R8G8B8A8_UNORM;
			case BASE_FORMAT::R8G8B8A8_SNORM:		return VK_FORMAT_R8G8B8A8_SNORM;
			case BASE_FORMAT::R8G8B8A8_UINT:		return VK_FORMAT_R8G8B8A8_UINT;
			case BASE_FORMAT::R8G8B8A8_SINT:		return VK_FORMAT_R8G8B8A8_SINT;
			case BASE_FORMAT::R8G8B8A8_SRGB:		return VK_FORMAT_R8G8B8A8_SRGB;
			case BASE_FORMAT::R16_UNORM:			return VK_FORMAT_R16_UNORM;
			case BASE_FORMAT::R16_SNORM:			return VK_FORMAT_R16_SNORM;
			case BASE_FORMAT::R16_UINT:				return VK_FORMAT_R16_UINT;
			case BASE_FORMAT::R16_SINT:				return VK_FORMAT_R16_SINT;
			case BASE_FORMAT::R16_FLOAT:			return VK_FORMAT_R16_SFLOAT;
			case BASE_FORMAT::R16G16_UNORM:			return VK_FORMAT_R16G16_UNORM;
			case BASE_FORMAT::R16G16_SNORM:			return VK_FORMAT_R16G16_SNORM;
			case BASE_FORMAT::R16G16_UINT:			return VK_FORMAT_R16G16_UINT;
			case BASE_FORMAT::R16G16_SINT:			return VK_FORMAT_R16G16_SINT;
			case BASE_FORMAT::R16G16_FLOAT:			return VK_FORMAT_R16G16_SFLOAT;
			case BASE_FORMAT::R16G16B16_UNORM:		return VK_FORMAT_R16G16B16_UNORM;
			case BASE_FORMAT::R16G16B16_SNORM:		return VK_FORMAT_R16G16B16_SNORM;
			case BASE_FORMAT::R16G16B16_UINT:		return VK_FORMAT_R16G16B16_UINT;
			case BASE_FORMAT::R16G16B16_SINT:		return VK_FORMAT_R16G16B16_SINT;
			case BASE_FORMAT::R16G16B16_FLOAT:		return VK_FORMAT_R16G16B16_SFLOAT;
			case BASE_FORMAT::R16G16B16A16_UNORM:	return VK_FORMAT_R16G16B16A16_UNORM;
			case BASE_FORMAT::R16G16B16A16_SNORM:	return VK_FORMAT_R16G16B16A16_SNORM;
			case BASE_FORMAT::R16G16B16A16_UINT:	return VK_FORMAT_R16G16B16A16_UINT;
			case BASE_FORMAT::R16G16B16A16_SINT:	return VK_FORMAT_R16G16B16A16_SINT;
			case BASE_FORMAT::R16G16B16A16_FLOAT:	return VK_FORMAT_R16G16B16A16_SFLOAT;
			case BASE_FORMAT::R32_UINT:				return VK_FORMAT_R32_UINT;
			case BASE_FORMAT::R32_SINT:				return VK_FORMAT_R32_SINT;
			case BASE_FORMAT::R32_FLOAT:			return VK_FORMAT_R32_SFLOAT;
			case BASE_FORMAT::R32G32_UINT:			return VK_FORMAT_R32G32_UINT;
			case BASE_FORMAT::R32G32_SINT:			return VK_FORMAT_R32G32_SINT;
			case BASE_FORMAT::R32G32_FLOAT:			return VK_FORMAT_R32G32_SFLOAT;
			case BASE_FORMAT::R32G32B32_UINT:		return VK_FORMAT_R32G32B32_UINT;
			case BASE_FORMAT::R32G32B32_SINT:		return VK_FORMAT_R32G32B32_SINT;
			case BASE_FORMAT::R32G32B32_FLOAT:		return VK_FORMAT_R32G32B32_SFLOAT;
			case BASE_FORMAT::R32G32B32A32_UINT:	return VK_FORMAT_R32G32B32A32_UINT;
			case BASE_FORMAT::R32G32B32A32_SINT:	return VK_FORMAT_R32G32B32A32_SINT;
			case BASE_FORMAT::R32G32B32A32_FLOAT:	return VK_FORMAT_R32G32B32A32_SFLOAT;
			case BASE_FORMAT::R64_UINT:				return VK_FORMAT_R64_UINT;
			case BASE_FORMAT::R64_SINT:				return VK_FORMAT_R64_SINT;
			case BASE_FORMAT::R64_FLOAT:			return VK_FORMAT_R64_SFLOAT;
			case BASE_FORMAT::R64G64_UINT:			return VK_FORMAT_R64G64_UINT;
			case BASE_FORMAT::R64G64_SINT:			return VK_FORMAT_R64G64_SINT;
			case BASE_FORMAT::R64G64_FLOAT:			return VK_FORMAT_R64G64_SFLOAT;
			case BASE_FORMAT::R64G64B64_UINT:		return VK_FORMAT_R64G64B64_UINT;
			case BASE_FORMAT::R64G64B64_SINT:		return VK_FORMAT_R64G64B64_SINT;
			case BASE_FORMAT::R64G64B64_FLOAT:		return VK_FORMAT_R64G64B64_SFLOAT;
			case BASE_FORMAT::R64G64B64A64_UINT:	return VK_FORMAT_R64G64B64A64_UINT;
			case BASE_FORMAT::R64G64B64A64_SINT:	return VK_FORMAT_R64G64B64A64_SINT;
			case BASE_FORMAT::R64G64B64A64_FLOAT:	return VK_FORMAT_R64G64B64A64_SFLOAT;
			case BASE_FORMAT::D16_UNORM:			return VK_FORMAT_D16_UNORM;
			case BASE_FORMAT::D32_FLOAT:			return VK_FORMAT_D32_SFLOAT;
			case BASE_FORMAT::S8_UINT:				return VK_FORMAT_S8_UINT;
			case BASE_FORMAT::D16_UNORM_S8_UINT:	return VK_FORMAT_D16_UNORM_S8_UINT;
			case BASE_FORMAT::D24_UNORM_S8_UINT:	return VK_FORMAT_D24_UNORM_S8_UINT;
			case BASE_FORMAT::D32_FLOAT_S8_UINT:	return VK_FORMAT_D32_SFLOAT_S8_UINT;
			case BASE_FORMAT::B8G8R8A8_SRGB:		return VK_FORMAT_B8G8R8A8_SRGB;
			}

			return VK_FORMAT_UNDEFINED;
		}

		static bool IsUsageFlagSet(UsageTypeFlags bits, USAGE_TYPE flag) { return IsFlagSet<USAGE_TYPE, UsageTypeFlags>(bits, flag); }
		VkImageUsageFlags ConvertUsageFlags(UsageTypeFlags flags)
		{
			if (flags == static_cast<UsageTypeFlags>(USAGE_TYPE::INVALID))
			{
				return VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM; // No good alernative for "INVALID"
			}

			VkImageUsageFlags res = 0;

			if (IsUsageFlagSet(flags, USAGE_TYPE::TRANSFER_SRC))
			{
				res |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::TRANSFER_DST))
			{
				res |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::SAMPLED))
			{
				res |= VK_IMAGE_USAGE_SAMPLED_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::STORAGE))
			{
				res |= VK_IMAGE_USAGE_STORAGE_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::COLOR_ATTACHMENT))
			{
				res |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::DEPTH_STENCIL_ATTACHMENT))
			{
				res |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::TRANSIENT_ATTACHMENT))
			{
				res |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			}
			if (IsUsageFlagSet(flags, USAGE_TYPE::INPUT_ATTACHMENT))
			{
				res |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			}

			return res;
		}

		VkSampleCountFlagBits ConvertSampleCount(SAMPLE_COUNT sampleCount)
		{
			switch (sampleCount)
			{
			case SAMPLE_COUNT::COUNT_1:  return VK_SAMPLE_COUNT_1_BIT;
			case SAMPLE_COUNT::COUNT_2:  return VK_SAMPLE_COUNT_2_BIT;
			case SAMPLE_COUNT::COUNT_4:  return VK_SAMPLE_COUNT_4_BIT;
			case SAMPLE_COUNT::COUNT_8:  return VK_SAMPLE_COUNT_8_BIT;
			case SAMPLE_COUNT::COUNT_16: return VK_SAMPLE_COUNT_16_BIT;
			case SAMPLE_COUNT::COUNT_32: return VK_SAMPLE_COUNT_32_BIT;
			case SAMPLE_COUNT::COUNT_64: return VK_SAMPLE_COUNT_64_BIT;

			case SAMPLE_COUNT::INVALID: break;
			}

			return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
		}

		VkImageViewType ConvertViewType(VIEW_TYPE type)
		{
			switch (type)
			{
			case VIEW_TYPE::TYPE_1D:         return VK_IMAGE_VIEW_TYPE_1D;
			case VIEW_TYPE::TYPE_1D_ARRAY:   return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
			case VIEW_TYPE::TYPE_2D:         return VK_IMAGE_VIEW_TYPE_2D;
			case VIEW_TYPE::TYPE_2D_ARRAY:   return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			case VIEW_TYPE::TYPE_3D:         return VK_IMAGE_VIEW_TYPE_3D;
			case VIEW_TYPE::TYPE_CUBE:       return VK_IMAGE_VIEW_TYPE_CUBE;
			case VIEW_TYPE::TYPE_CUBE_ARRAY: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

			case VIEW_TYPE::INVALID: break;
			}

			return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		}

		static bool IsAspectFlagSet(AspectTypeFlags bits, ASPECT_TYPE flag) { return IsFlagSet<ASPECT_TYPE, AspectTypeFlags>(bits, flag); }
		VkImageAspectFlags ConvertAspectFlags(AspectTypeFlags flags)
		{
			if (flags == static_cast<AspectTypeFlags>(ASPECT_TYPE::INVALID))
			{
				return VK_IMAGE_ASPECT_NONE;
			}

			AspectTypeFlags res = 0;

			if (IsAspectFlagSet(flags, ASPECT_TYPE::COLOR))
			{
				res |= VK_IMAGE_ASPECT_COLOR_BIT;
			}
			if (IsAspectFlagSet(flags, ASPECT_TYPE::DEPTH))
			{
				res |= VK_IMAGE_ASPECT_DEPTH_BIT;
			}
			if (IsAspectFlagSet(flags, ASPECT_TYPE::STENCIL))
			{
				res |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}

			return res;
		}

		VkFilter ConvertFilterMode(FILTER_MODE filterMode)
		{
			switch (filterMode)
			{
			case FILTER_MODE::LINEAR:  return VK_FILTER_LINEAR;
			case FILTER_MODE::NEAREST: return VK_FILTER_NEAREST;

			case FILTER_MODE::INVALID: break;
			}

			return VK_FILTER_MAX_ENUM;
		}

		VkSamplerMipmapMode ConvertMipMapMode(FILTER_MODE filterMode)
		{
			switch (filterMode)
			{
			case FILTER_MODE::LINEAR:  return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			case FILTER_MODE::NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;

			case FILTER_MODE::INVALID: break;
			}

			return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
		}

		VkSamplerAddressMode ConvertAddressMode(SAMPLER_ADDRESS_MODE addressMode)
		{
			switch (addressMode)
			{
			case SAMPLER_ADDRESS_MODE::REPEAT:					return VK_SAMPLER_ADDRESS_MODE_REPEAT;
			case SAMPLER_ADDRESS_MODE::MIRRORED_REPEAT:			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			case SAMPLER_ADDRESS_MODE::CLAMP_TO_EDGE:			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			case SAMPLER_ADDRESS_MODE::CLAMP_TO_BORDER:			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			case SAMPLER_ADDRESS_MODE::MIRRORED_CLAMP_TO_EDGE:	return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;

			case SAMPLER_ADDRESS_MODE::INVALID: break;
			}

			return VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;
		}

		BASE_FORMAT ConvertSurfaceFormat(VkFormat format)
		{
			// TODO - Fill out more supported surface formats
			switch (format)
			{
			case VK_FORMAT_B8G8R8A8_SRGB: return BASE_FORMAT::B8G8R8A8_SRGB;
			}

			return BASE_FORMAT::INVALID;
		}
	}
}