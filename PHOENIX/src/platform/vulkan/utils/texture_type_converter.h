#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/texture_desc.h"

namespace PHX
{
	namespace TEX_UTILS
	{
		VkFormat ConvertTextureFormat(TEXTURE_FORMAT format);
		VkImageUsageFlags ConvertUsageFlags(UsageTypeFlags flags);
		VkSampleCountFlagBits ConvertSampleCount(SAMPLE_COUNT sampleCount);
		VkImageViewType ConvertViewType(VIEW_TYPE type);
		VkImageAspectFlags ConvertAspectFlags(AspectTypeFlags flags);

		TEXTURE_FORMAT ConvertSurfaceFormat(VkFormat format);
	}
}