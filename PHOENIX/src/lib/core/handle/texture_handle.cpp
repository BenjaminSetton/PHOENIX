
#include "PHX/interface/texture.h"
#include "PHX/interface/render_device.h"

namespace PHX
{
	TextureHandle::TextureHandle() : Handle(HANDLE_TYPE::TEXTURE)
	{
	}

	TextureHandle::TextureHandle(const Handle& other) : Handle(other)
	{
	}

	TextureHandle::~TextureHandle()
	{
	}

	TextureHandle::TextureHandle(const TextureHandle& other) : Handle(other)
	{
	}

	TextureHandle& TextureHandle::operator=(const TextureHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator==(other);
		return *this;
	}

	TextureHandle::TextureHandle(TextureHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	void TextureHandle::CopyFrom(TextureHandle other)
	{
		ITexture* pThisTexture = m_pRenderDevice->ResolveHandle(*this);
		ITexture* pOtherTexture = m_pRenderDevice->ResolveHandle(other);

		if (pThisTexture != nullptr && pOtherTexture != nullptr)
		{
			pThisTexture->CopyFrom(pOtherTexture);
		}
	}

	u32 TextureHandle::GetWidth() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetWidth();
		}

		return 0;
	}

	u32 TextureHandle::GetHeight() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetHeight();
		}

		return 0;
	}

	BASE_FORMAT TextureHandle::GetFormat() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetFormat();
		}

		return BASE_FORMAT::INVALID;
	}

	u32 TextureHandle::GetArrayLayers() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetArrayLayers();
		}

		return 0;
	}

	u32 TextureHandle::GetMipLevels() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetMipLevels();
		}

		return 0;
	}

	SAMPLE_COUNT TextureHandle::GetSampleCount() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetSampleCount();
		}

		return SAMPLE_COUNT::INVALID;
	}

	AspectTypeFlags TextureHandle::GetAspectFlags() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetAspectFlags();
		}

		return 0;
	}

	VIEW_TYPE TextureHandle::GetViewType() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetViewType();
		}

		return VIEW_TYPE::INVALID;
	}

	VIEW_SCOPE TextureHandle::GetViewScope() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetViewScope();
		}

		return VIEW_SCOPE::INVALID;
	}

	FILTER_MODE TextureHandle::GetMinificationFilter() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetMinificationFilter();
		}

		return FILTER_MODE::INVALID;
	}

	FILTER_MODE TextureHandle::GetMagnificationFilter() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetMagnificationFilter();
		}

		return FILTER_MODE::INVALID;
	}

	SAMPLER_ADDRESS_MODE TextureHandle::GetSamplerAddressMode() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetSamplerAddressMode();
		}

		return SAMPLER_ADDRESS_MODE::INVALID;
	}

	FILTER_MODE TextureHandle::GetSamplerFilter() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetSamplerFilter();
		}

		return FILTER_MODE::INVALID;
	}

	bool TextureHandle::IsAnisotropicFilteringEnabled() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->IsAnisotropicFilteringEnabled();
		}

		return false;
	}

	float TextureHandle::GetAnisotropyLevel() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->GetAnisotropyLevel();
		}

		return 0.0f;
	}

	bool TextureHandle::IsDepthTexture() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->IsDepthTexture();
		}

		return false;
	}

	bool TextureHandle::HasStencilComponent() const
	{
		ITexture* pTexture = m_pRenderDevice->ResolveHandle(*this);
		if (pTexture != nullptr)
		{
			return pTexture->HasStencilComponent();
		}

		return false;
	}
}