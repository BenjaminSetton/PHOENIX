#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/types/attachment_desc.h"
#include "PHX/types/status_code.h"
#include "texture_vk.h"
#include "utils/render_pass_cache.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	struct FramebufferAttachmentDesc
	{
		TextureVk* pTexture = nullptr;
		u32 mipTarget = 0;
		ATTACHMENT_TYPE type = ATTACHMENT_TYPE::INVALID;
		ATTACHMENT_LOAD_OP loadOp = ATTACHMENT_LOAD_OP::INVALID;
		ATTACHMENT_STORE_OP storeOp = ATTACHMENT_STORE_OP::INVALID;
	};

	struct FramebufferDescription
	{
		u32 width = 0;
		u32 height = 0;
		u32 layers = 0;
		FramebufferAttachmentDesc* pAttachments = nullptr;
		u32 attachmentCount = 0;
		VkRenderPass renderPass;

		////////
		bool operator==(const FramebufferDescription& other) const;
		////////
	};

	class FramebufferVk
	{
	public:

		explicit FramebufferVk(RenderDeviceVk* pRenderDevice, const FramebufferDescription& desc);
		~FramebufferVk();

		u32 GetWidth();
		u32 GetHeight();
		u32 GetLayers();
		u32 GetAttachmentCount();
		TextureVk* GetAttachment(u32 index);

		VkRenderPass GetRenderPass() const;

		VkFramebuffer GetFramebuffer() const;

	private:

		STATUS_CODE VerifyDescription(const FramebufferDescription& desc);

	private:

		VkFramebuffer m_framebuffer;
		std::vector<TextureVk*> m_attachments;
		u32 m_width;
		u32 m_height;
		u32 m_layers;

		// Stores the render pass that is linked to this framebuffer
		VkRenderPass m_renderPass;
	};
}