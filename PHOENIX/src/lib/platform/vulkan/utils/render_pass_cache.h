#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PHX/types/integral_types.h"
#include "render_pass_helpers.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	// TODO - Step away from singleton pattern, and instead have this cache as a private member variable that the render device owns!
	class RenderPassCache
	{
	public:

		explicit RenderPassCache(RenderDeviceVk* pRenderDevice);
		~RenderPassCache();

		VkRenderPass Find(const RenderPassDescription& desc) const;
		VkRenderPass FindOrCreate(RenderDeviceVk* pRenderDevice, const RenderPassDescription& desc);
		void Delete(const RenderPassDescription& desc);

	private:

		VkRenderPass CreateFromDescription(RenderDeviceVk* pRenderDevice, const RenderPassDescription& desc) const;

	private:

		RenderDeviceVk* m_pRenderDevice;

		std::unordered_map<RenderPassDescription, VkRenderPass, RenderPassDescriptionHasher> m_cache;
	};
}