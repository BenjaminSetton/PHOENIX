#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/framebuffer.h"
#include "PHX/types/status_code.h"
#include "utils/render_pass_cache.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class FramebufferVk : public IFramebuffer
	{
	public:

		explicit FramebufferVk(RenderDeviceVk* pRenderDevice, const FramebufferCreateInfo& createInfo);
		~FramebufferVk() override;

		u32 GetWidth() override;
		u32 GetHeight() override;
		u32 GetLayers() override;
		u32 GetAttachmentCount() override;
		ITexture* GetAttachment(u32 index) override;

		const RenderPassDescription& GetRenderPassDescription() const;

	private:

		STATUS_CODE VerifyCreateInfo(const FramebufferCreateInfo& createInfo);

	private:

		VkFramebuffer m_framebuffer;
		std::vector<ITexture*> m_attachments;
		u32 m_width;
		u32 m_height;
		u32 m_layers;

		// Stores a description of the render pass that is linked to this framebuffer
		RenderPassDescription m_renderPassDesc;
	};
}