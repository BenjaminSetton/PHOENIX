#pragma once

#include "../types/integral_types.h"
#include "../types/texture_desc.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;

	struct TextureBaseCreateInfo
	{
		u32 width                 = 0;
		u32 height                = 0;
		BASE_FORMAT format        = BASE_FORMAT::INVALID;
		u32 arrayLayers           = 1;
		u32 mipLevels             = 1;
		UsageTypeFlags usageFlags = 0;
		SAMPLE_COUNT sampleFlags  = SAMPLE_COUNT::COUNT_1;
		bool generateMips         = false;
	};

	struct TextureViewCreateInfo
	{
		VIEW_TYPE type         = VIEW_TYPE::INVALID;
		VIEW_SCOPE scope       = VIEW_SCOPE::INVALID;
		AspectTypeFlags aspectFlags = 0;
	};

	struct TextureSamplerCreateInfo
	{
		FILTER_MODE minificationFilter      = FILTER_MODE::INVALID;
		FILTER_MODE magnificationFilter     = FILTER_MODE::INVALID;
		SAMPLER_ADDRESS_MODE addressModeUVW = SAMPLER_ADDRESS_MODE::INVALID;
		FILTER_MODE samplerMipMapFilter     = FILTER_MODE::INVALID;
		bool enableAnisotropicFiltering     = true;
		float maxAnisotropy                 = 1.0f;
	};

	class ITexture
	{
	public:

		virtual ~ITexture() { }

		// All texture copies must be explicitly made, since copy constructor and copy-assignment
		// are deleted functions
		virtual void CopyFrom(ITexture* other) = 0;

		virtual u32 GetWidth() const = 0;
		virtual u32 GetHeight() const = 0;
		virtual BASE_FORMAT GetFormat() const = 0;
		virtual u32 GetArrayLayers() const = 0;
		virtual u32 GetMipLevels() const = 0;
		virtual SAMPLE_COUNT GetSampleCount() const = 0;

		virtual VIEW_TYPE GetViewType() const = 0;
		virtual VIEW_SCOPE GetViewScope() const = 0;

		virtual FILTER_MODE GetMinificationFilter() const = 0;
		virtual FILTER_MODE GetMagnificationFilter() const = 0;
		virtual SAMPLER_ADDRESS_MODE GetSamplerAddressMode() const = 0;
		virtual FILTER_MODE GetSamplerFilter() const = 0;
		virtual bool IsAnisotropicFilteringEnabled() const = 0;
		virtual float GetAnisotropyLevel() const = 0;

		virtual bool IsDepthTexture() const = 0;
		virtual bool HasStencilComponent() const = 0;
	};
}