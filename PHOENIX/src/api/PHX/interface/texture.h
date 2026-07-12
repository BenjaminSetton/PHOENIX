#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/texture_desc.h"

#include "PHX/interface/handle.h"

namespace PHX
{
	struct TextureBaseCreateInfo
	{
		const char* pName         = "";
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

	struct TextureHandle : public Handle
	{
		DECLARE_PHX_HANDLE(TextureHandle);

		void CopyFrom(TextureHandle other);
		u32 GetWidth() const;
		u32 GetHeight() const;
		BASE_FORMAT GetFormat() const;
		u32 GetArrayLayers() const;
		u32 GetMipLevels() const;
		SAMPLE_COUNT GetSampleCount() const;
		AspectTypeFlags GetAspectFlags() const;
		VIEW_TYPE GetViewType() const;
		VIEW_SCOPE GetViewScope() const;
		FILTER_MODE GetMinificationFilter() const;
		FILTER_MODE GetMagnificationFilter() const;
		SAMPLER_ADDRESS_MODE GetSamplerAddressMode() const;
		FILTER_MODE GetSamplerFilter() const;
		bool IsAnisotropicFilteringEnabled() const;
		float GetAnisotropyLevel() const;
		bool IsDepthTexture() const;
		bool HasStencilComponent() const;
	};
}