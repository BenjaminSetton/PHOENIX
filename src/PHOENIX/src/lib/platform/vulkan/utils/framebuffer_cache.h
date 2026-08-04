#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>

#include "../framebuffer_vk.h"
#include "utils/cache_utils.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	// Used to hash custom key type for cache
	struct FramebufferDescriptionHasher
	{
		size_t operator()(const FramebufferDescription& desc) const;
	};

	class FramebufferCache
	{
	public:

		using UnderlyingCacheType = std::unordered_map<FramebufferDescription, FramebufferVk*, FramebufferDescriptionHasher>;
		using CacheIterator = UnderlyingCacheType::const_iterator;

	public:

		FramebufferCache();
		~FramebufferCache();

		FramebufferVk* Find(const FramebufferDescription& desc) const;
		FramebufferVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const FramebufferDescription& desc);
		void Delete(const FramebufferDescription& desc);

		CacheIterator Begin();
		CacheIterator End();

	private:

		UnderlyingCacheType m_cache;
	};
}