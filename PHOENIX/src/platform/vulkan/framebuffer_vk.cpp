
#include "framebuffer_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "render_device_vk.h"
#include "texture_vk.h"
#include "utils/render_pass_cache.h"

namespace PHX
{
	static RenderPassDescription BuildRenderPassDescription(const FramebufferCreateInfo& info)
	{
		RenderPassDescription rpDesc{};

		TODO();

		return rpDesc;
	}

	FramebufferVk::FramebufferVk(RenderDeviceVk* pRenderDevice, const FramebufferCreateInfo& createInfo)
	{
		if (VerifyCreateInfo(createInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		std::vector<VkImageView> imageViews;
		for (u32 i = 0; i < createInfo.attachmentCount; i++)
		{
			u32 mipTarget = createInfo.pMipTargets[i];
			TextureVk* texVk = dynamic_cast<TextureVk*>(createInfo.pAttachments[i]);
			VkImageView imageView = texVk->GetImageViewAt(mipTarget);
			if (imageView == VK_NULL_HANDLE)
			{
				LogWarning("Mip target at index %u doesn't exist! Skipping target during framebuffer creation", i);
				continue;
			}

			imageViews.push_back(imageView);
		}



		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = RenderPassCache::Get().Insert(pRenderDevice, BuildRenderPassDescription(createInfo));
		framebufferInfo.attachmentCount = createInfo.attachmentCount;
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = createInfo.width;
		framebufferInfo.height = createInfo.height;
		framebufferInfo.layers = createInfo.layers;
		if (vkCreateFramebuffer(pRenderDevice->GetLogicalDevice(), &framebufferInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
		{
			LogError("Failed to create framebuffer!");
			return;
		}

		// Cache the attachments
		m_attachments.reserve(createInfo.attachmentCount);
		for (u32 i = 0; i < createInfo.attachmentCount; i++)
		{
			m_attachments.push_back(createInfo.pAttachments[i]);
		}
		m_width = createInfo.width;
		m_height = createInfo.height;
		m_layers = createInfo.layers;
	}

	FramebufferVk::~FramebufferVk()
	{
		m_attachments.clear();
	}

	u32 FramebufferVk::GetWidth()
	{
		return m_width;
	}

	u32 FramebufferVk::GetHeight()
	{
		return m_height;
	}

	u32 FramebufferVk::GetLayers()
	{
		return m_layers;
	}

	u32 FramebufferVk::GetAttachmentCount()
	{
		return static_cast<u32>(m_attachments.size());
	}

	STATUS_CODE FramebufferVk::VerifyCreateInfo(const FramebufferCreateInfo& createInfo)
	{
		if (m_framebuffer != VK_NULL_HANDLE)
		{
			LogError("Attempting to create a framebuffer twice!");
			return STATUS_CODE::ERR;
		}

		if (createInfo.attachmentCount == 0)
		{
			LogError("Attempting to create a framebuffer with no attachments!");
			return STATUS_CODE::ERR;
		}

		if (createInfo.width == 0 || createInfo.height == 0 || createInfo.layers == 0)
		{
			LogError("Attempting to create a framebuffer with no width, height or layer count!");
			return STATUS_CODE::ERR;
		}

		if (createInfo.pAttachments == nullptr || createInfo.attachmentCount == 0)
		{
			LogError("Attempting to create a framebuffer with no attachments!");
			return STATUS_CODE::ERR;
		}

		if (createInfo.pMipTargets == nullptr || createInfo.mipTargetCount == 0)
		{
			LogError("Attempting to create a framebuffer with no mip targets!");
			return STATUS_CODE::ERR;
		}

		if (createInfo.mipTargetCount != createInfo.attachmentCount)
		{
			LogError("Attempting to create a framebuffer where the mip target count and attachment count are not equal!");
			return STATUS_CODE::ERR;
		}

		//if (createInfo.renderPass == nullptr)
		//{
		//	LogError("Attempting to create framebuffer with an invalid render pass. Render pass is null");
		//	return;
		//}

		return STATUS_CODE::SUCCESS;
	}

}