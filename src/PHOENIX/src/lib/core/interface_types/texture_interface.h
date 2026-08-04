#pragma once

#include "BSL/integral_types.h"
#include "core/ref.h"
#include "PHX/types/texture_desc.h"

namespace PHX
{
	class ITexture : public RefCounted
	{
	public:

		virtual ~ITexture() { }

		// All texture copies must be explicitly made, since copy constructor and copy-assignment
		// are deleted functions
		virtual void CopyFrom(ITexture* other) = 0;

		virtual const char* GetName() const = 0;
		virtual u32 GetWidth() const = 0;
		virtual u32 GetHeight() const = 0;
		virtual BASE_FORMAT GetFormat() const = 0;
		virtual u32 GetArrayLayers() const = 0;
		virtual u32 GetMipLevels() const = 0;
		virtual SAMPLE_COUNT GetSampleCount() const = 0;

		virtual AspectTypeFlags GetAspectFlags() const = 0;
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