#pragma once

#include "../../../utils/sanity.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
	enum class VIEW_SCOPE
	{
		INVALID = 0,

		ENTIRE, // Generates an image view for the entire image, including all mip levels and cubemap faces (in cases where image is a cubemap)
		PER_MIP	// Generates an image view for every mip level
	};

	enum class FILTER_MODE
	{
		INVALID = 0,

		NEAREST,
		LINEAR
	};

	enum class SAMPLER_ADDRESS_MODE
	{
		INVALID = 0,

		REPEAT,
		MIRRORED_REPEAT,
		CLAMP_TO_EDGE,
		CLAMP_TO_BORDER,
		MIRRORED_CLAMP_TO_EDGE
	};

	enum class USAGE_TYPE : u8
	{
		INVALID = 0,

		TRANSFER_SRC             = BIT(0),
		TRANSFER_DST             = BIT(1),
		SAMPLED                  = BIT(2),
		STORAGE                  = BIT(3),
		COLOR_ATTACHMENT         = BIT(4),
		DEPTH_STENCIL_ATTACHMENT = BIT(5),
		TRANSIENT_ATTACHMENT     = BIT(6),
		INPUT_ATTACHMENT         = BIT(7)
	};
	static constexpr u8 USAGE_TYPE_MAX = 8u;
	typedef u8 UsageTypeFlags;

	enum class SAMPLE_COUNT : u8
	{
		INVALID = 0,

		COUNT_1  = BIT(0),
		COUNT_2  = BIT(1),
		COUNT_4  = BIT(2),
		COUNT_8  = BIT(3),
		COUNT_16 = BIT(4),
		COUNT_32 = BIT(5),
		COUNT_64 = BIT(6)
	};
	static constexpr u8 SAMPLE_COUNT_MAX = 7u;
	typedef u8 SampleCountFlags;

	enum class VIEW_TYPE
	{
		INVALID = 0,

		TYPE_1D,
		TYPE_2D,
		TYPE_3D,
		TYPE_CUBE,
		TYPE_1D_ARRAY,
		TYPE_2D_ARRAY,
		TYPE_CUBE_ARRAY
	};

	enum class ASPECT_TYPE : u8
	{
		INVALID = 0,

		COLOR   = BIT(0),
		DEPTH   = BIT(1),
		STENCIL = BIT(2)
	};
	static constexpr u8 ASPECT_TYPE_MAX = 3u;
	typedef u8 AspectTypeFlags;

	enum class TEXTURE_FORMAT
	{
		INVALID = 0,

		// 8-bit component RGBA
		R8_UNORM,
		R8_SNORM,
		R8_UINT,
		R8_SINT,
		R8G8_UNORM,
		R8G8_SNORM,
		R8G8_UINT,
		R8G8_SINT,
		R8G8B8_UNORM,
		R8G8B8_SNORM,
		R8G8B8_UINT,
		R8G8B8_SINT,
		R8G8B8A8_UNORM,
		R8G8B8A8_SNORM,
		R8G8B8A8_UINT,
		R8G8B8A8_SINT,

		// 16-bit component RGBA
		R16_UNORM,
		R16_SNORM,
		R16_UINT,
		R16_SINT,
		R16_FLOAT,
		R16G16_UNORM,
		R16G16_SNORM,
		R16G16_UINT,
		R16G16_SINT,
		R16G16_FLOAT,
		R16G16B16_UNORM,
		R16G16B16_SNORM,
		R16G16B16_UINT,
		R16G16B16_SINT,
		R16G16B16_FLOAT,
		R16G16B16A16_UNORM,
		R16G16B16A16_SNORM,
		R16G16B16A16_UINT,
		R16G16B16A16_SINT,
		R16G16B16A16_FLOAT,

		// 32-bit component RGBA
		R32_UINT,
		R32_SINT,
		R32_FLOAT,
		R32G32_UINT,
		R32G32_SINT,
		R32G32_FLOAT,
		R32G32B32_UINT,
		R32G32B32_SINT,
		R32G32B32_FLOAT,
		R32G32B32A32_UINT,
		R32G32B32A32_SINT,
		R32G32B32A32_FLOAT,

		// 64-bit component RGBA
		R64_UINT,
		R64_SINT,
		R64_FLOAT,
		R64G64_UINT,
		R64G64_SINT,
		R64G64_FLOAT,
		R64G64B64_UINT,
		R64G64B64_SINT,
		R64G64B64_FLOAT,
		R64G64B64A64_UINT,
		R64G64B64A64_SINT,
		R64G64B64A64_FLOAT,
		
		// Depth / stencil
		D16_UNORM,
		D32_FLOAT,
		S8_UINT,
		D16_UNORM_S8_UINT,
		D24_UNORM_S8_UINT,
		D32_FLOAT_S8_UINT
	};
}