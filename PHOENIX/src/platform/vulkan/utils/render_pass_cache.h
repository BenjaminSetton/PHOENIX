#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PHX/types/integral_types.h"
#include "render_pass_helpers.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class RenderPassCache
	{
	public:

		static RenderPassCache& Get()
		{
			static RenderPassCache instance;
			return instance;
		}

		// Finds the VkRenderPass associated with the RenderPassDescription object and returns it.
		// If no render pass is found, return VK_NULL_HANDLE instead
		VkRenderPass Find(const RenderPassDescription& desc) const;

		// Internally creates the VkRenderPass using the provided description.
		VkRenderPass Insert(RenderDeviceVk* pRenderPass, const RenderPassDescription& desc);

		// Deletes the VkRenderPass associated with the provided description, if any
		void Delete(const RenderPassDescription& desc);

	private:

		RenderPassCache();

		VkRenderPass CreateFromDescription(RenderDeviceVk* pRenderDevice, const RenderPassDescription& desc) const;

	private:

		std::unordered_map<RenderPassDescription, VkRenderPass, RenderPassDescriptionHasher> m_cache;
	};
}