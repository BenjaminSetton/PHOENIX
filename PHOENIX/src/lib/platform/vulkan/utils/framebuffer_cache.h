#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>

#include "../framebuffer_vk.h"

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

		FramebufferCache() = default;
		~FramebufferCache() = default;

		FramebufferVk* Find(const FramebufferDescription& desc) const;
		FramebufferVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const FramebufferDescription& desc);
		void Delete(const FramebufferDescription& desc);

	private:

		std::unordered_map<FramebufferDescription, FramebufferVk*, FramebufferDescriptionHasher> m_cache;
	};
}