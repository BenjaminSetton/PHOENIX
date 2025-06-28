
#include <cmath> // log2
#include <utility>

#include "texture_utils.h"

#include "utils/logger.h"

namespace PHX
{
	u32 CalculateMipLevelsFromSize(u32 width, u32 height)
	{
		double dWidth = static_cast<double>(width);
		double dHeight = static_cast<double>(height);
		double exactMips = log2(std::min(dWidth, dHeight));
		return static_cast<u32>(exactMips);
	}

	u32 GetBaseFormatSize(BASE_FORMAT format)
	{
		switch (format)
		{
		case BASE_FORMAT::INVALID:
		{
			return 0;
		}
		case BASE_FORMAT::R8_UNORM:
		case BASE_FORMAT::R8_SNORM:
		case BASE_FORMAT::R8_UINT:
		case BASE_FORMAT::R8_SINT:
		case BASE_FORMAT::R8_SRGB:
		case BASE_FORMAT::S8_UINT:
		{
			return 1;
		}
		case BASE_FORMAT::R8G8_UNORM:
		case BASE_FORMAT::R8G8_SNORM:
		case BASE_FORMAT::R8G8_UINT:
		case BASE_FORMAT::R8G8_SINT:
		case BASE_FORMAT::R8G8_SRGB:
		case BASE_FORMAT::R16_UNORM:
		case BASE_FORMAT::R16_SNORM:
		case BASE_FORMAT::R16_UINT:
		case BASE_FORMAT::R16_SINT:
		case BASE_FORMAT::R16_FLOAT:
		case BASE_FORMAT::D16_UNORM:
		{
			return 2;
		}
		case BASE_FORMAT::R8G8B8_UNORM:
		case BASE_FORMAT::R8G8B8_SNORM:
		case BASE_FORMAT::R8G8B8_UINT:
		case BASE_FORMAT::R8G8B8_SINT:
		case BASE_FORMAT::R8G8B8_SRGB:
		case BASE_FORMAT::D16_UNORM_S8_UINT:
		{
			return 3;
		}
		case BASE_FORMAT::R8G8B8A8_UNORM:
		case BASE_FORMAT::R8G8B8A8_SNORM:
		case BASE_FORMAT::R8G8B8A8_UINT:
		case BASE_FORMAT::R8G8B8A8_SINT:
		case BASE_FORMAT::R8G8B8A8_SRGB:
		case BASE_FORMAT::R16G16_UNORM:
		case BASE_FORMAT::R16G16_SNORM:
		case BASE_FORMAT::R16G16_UINT:
		case BASE_FORMAT::R16G16_SINT:
		case BASE_FORMAT::R16G16_FLOAT:
		case BASE_FORMAT::R32_UINT:
		case BASE_FORMAT::R32_SINT:
		case BASE_FORMAT::R32_FLOAT:
		case BASE_FORMAT::D32_FLOAT:
		case BASE_FORMAT::D24_UNORM_S8_UINT:
		case BASE_FORMAT::B8G8R8A8_SRGB:
		{
			return 4;
		}
		case BASE_FORMAT::D32_FLOAT_S8_UINT:
		{
			return 5;
		}
		case BASE_FORMAT::R16G16B16_UNORM:
		case BASE_FORMAT::R16G16B16_SNORM:
		case BASE_FORMAT::R16G16B16_UINT:
		case BASE_FORMAT::R16G16B16_SINT:
		case BASE_FORMAT::R16G16B16_FLOAT:
		{
			return 6;
		}
		case BASE_FORMAT::R16G16B16A16_UNORM:
		case BASE_FORMAT::R16G16B16A16_SNORM:
		case BASE_FORMAT::R16G16B16A16_UINT:
		case BASE_FORMAT::R16G16B16A16_SINT:
		case BASE_FORMAT::R16G16B16A16_FLOAT:
		case BASE_FORMAT::R32G32_UINT:
		case BASE_FORMAT::R32G32_SINT:
		case BASE_FORMAT::R32G32_FLOAT:
		case BASE_FORMAT::R64_UINT:
		case BASE_FORMAT::R64_SINT:
		case BASE_FORMAT::R64_FLOAT:
		{
			return 8;
		}
		case BASE_FORMAT::R32G32B32_UINT:
		case BASE_FORMAT::R32G32B32_SINT:
		case BASE_FORMAT::R32G32B32_FLOAT:
		{
			return 12;
		}
		case BASE_FORMAT::R32G32B32A32_UINT:
		case BASE_FORMAT::R32G32B32A32_SINT:
		case BASE_FORMAT::R32G32B32A32_FLOAT:
		case BASE_FORMAT::R64G64_UINT:
		case BASE_FORMAT::R64G64_SINT:
		case BASE_FORMAT::R64G64_FLOAT:
		{
			return 16;
		}
		case BASE_FORMAT::R64G64B64_UINT:
		case BASE_FORMAT::R64G64B64_SINT:
		case BASE_FORMAT::R64G64B64_FLOAT:
		{
			return 24;
		}
		case BASE_FORMAT::R64G64B64A64_UINT:
		case BASE_FORMAT::R64G64B64A64_SINT:
		case BASE_FORMAT::R64G64B64A64_FLOAT:
		{
			return 32;
		}
		}

		LogError("Failed to calculate bytes per texel for format %u", static_cast<u32>(format));
		return 0;
	}
}