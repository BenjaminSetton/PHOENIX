#pragma once

#include "PHX/interface/texture.h"

namespace PHX
{
	class TextureVk : public ITexture
	{
	public:

		TextureVk(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo);
		~TextureVk();

		u32 GetWidth() const;
		u32 GetHeight() const;
		TEXTURE_FORMAT GetFormat() const;
		u32 GetArrayLayers() const;
		u32 GetMipLevels() const;

		VIEW_TYPE GetViewType() const;
		VIEW_SCOPE GetViewScope() const;

		FILTER_MODE GetMinificationFilter() const;
		FILTER_MODE GetMagnificationFilter() const;
		SAMPLER_ADDRESS_MODE GetSamplerAddressMode() const;
		FILTER_MODE GetSamplerFilter() const;
		bool IsAnisotropicFilteringEnabled() const;
		float GetAnisotropyLevel() const;

	private:

		u32 m_width;
		u32 m_height;
		TEXTURE_FORMAT m_format;
		u32 m_arrayLayers;
		u32 m_mipLevels;
	};
}