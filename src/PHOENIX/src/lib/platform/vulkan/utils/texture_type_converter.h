#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/texture_desc.h"

namespace PHX
{
	namespace TEX_UTILS
	{
		VkFormat ConvertBaseFormat(BASE_FORMAT format);
		VkImageUsageFlags ConvertUsageFlags(UsageTypeFlags flags);
		VkSampleCountFlagBits ConvertSampleCount(SAMPLE_COUNT sampleCount);
		VkImageViewType ConvertViewType(VIEW_TYPE type);
		VkImageAspectFlags ConvertAspectFlags(AspectTypeFlags flags);
		VkFilter ConvertFilterMode(FILTER_MODE filterMode);
		VkSamplerMipmapMode ConvertMipMapMode(FILTER_MODE filterMode);
		VkSamplerAddressMode ConvertAddressMode(SAMPLER_ADDRESS_MODE addressMode);

		BASE_FORMAT ConvertSurfaceFormat(VkFormat format);
	}
}