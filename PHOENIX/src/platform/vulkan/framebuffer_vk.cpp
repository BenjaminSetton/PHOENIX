
#include "framebuffer_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "render_device_vk.h"
#include "texture_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/render_pass_cache.h"

namespace PHX
{
	static RenderPassDescription BuildRenderPassDescription(const FramebufferCreateInfo& info)
	{
		RenderPassDescription rpDesc{};

		rpDesc.attachments.resize(info.attachmentCount);
		rpDesc.subpasses.resize(1); // Just using 1 subpass for now

		SubpassDescription& subpassDesc = rpDesc.subpasses.at(0);

		subpassDesc.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Does compute use framebuffers at all?
		subpassDesc.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // TODO - optimize
		subpassDesc.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // TODO - optimize
		subpassDesc.dstAccessMask = VK_ACCESS_NONE;

		for (u32 i = 0; i < info.attachmentCount; i++)
		{
			FramebufferAttachmentDesc& framebufferAttDesc = info.pAttachments[i];
			AttachmentDescription& attDesc = rpDesc.attachments.at(i);

			attDesc.pTexture = dynamic_cast<TextureVk*>(framebufferAttDesc.pTexture);

			VkAttachmentLoadOp loadOp = ATT_UTILS::ConvertLoadOp(framebufferAttDesc.loadOp);
			VkAttachmentStoreOp storeOp = ATT_UTILS::ConvertStoreOp(framebufferAttDesc.storeOp);

			switch (framebufferAttDesc.type)
			{
			case ATTACHMENT_TYPE::COLOR:
			{
				attDesc.loadOp = loadOp;
				attDesc.storeOp = storeOp;

				// How inefficient is this?
				attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				subpassDesc.colorAttachmentIndices.push_back(i);
				break;
			}
			case ATTACHMENT_TYPE::DEPTH:
			{
				attDesc.loadOp = loadOp;
				attDesc.storeOp = storeOp;

				// How inefficient is this?
				attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

				if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
				{
					LogWarning("Attempting to assign depth attachment to framebuffer more than once! Depth attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
				}
				else
				{
					subpassDesc.depthStencilAttachmentIndex = i;
				}

				break;
			}
			case ATTACHMENT_TYPE::STENCIL:
			{
				attDesc.stencilLoadOp = loadOp;
				attDesc.stencilStoreOp = storeOp;

				// How inefficient is this?
				attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.layout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;

				if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
				{
					LogWarning("Attempting to assign stencil attachment to framebuffer more than once! Stencil attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
				}
				else
				{
					subpassDesc.depthStencilAttachmentIndex = i;
				}

				break;
			}
			case ATTACHMENT_TYPE::DEPTH_STENCIL:
			{
				attDesc.stencilLoadOp = loadOp;
				attDesc.stencilStoreOp = storeOp;

				// How inefficient is this?
				attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
				{
					LogWarning("Attempting to assign depth-stencil attachment to framebuffer more than once! Depth-stencil attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
				}
				else
				{
					subpassDesc.depthStencilAttachmentIndex = i;
				}

				break;
			}
			case ATTACHMENT_TYPE::RESOLVE:
			{
				attDesc.loadOp = loadOp;
				attDesc.storeOp = storeOp;

				attDesc.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
				attDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Assuming it'll be presented right after resolving
				attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				if (subpassDesc.resolveAttachmentIndex != U32_MAX)
				{
					LogWarning("Attempting to assign resolve attachment to framebuffer more than once! Resolve attachment is already assigned to index %u", subpassDesc.resolveAttachmentIndex);
				}
				else
				{
					subpassDesc.resolveAttachmentIndex = i;
				}

				break;
			}
			}
		}

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
			FramebufferAttachmentDesc& attachmentDesc = createInfo.pAttachments[i];
			u32 mipTarget = attachmentDesc.mipTarget;
			TextureVk* texVk = dynamic_cast<TextureVk*>(attachmentDesc.pTexture);
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
			m_attachments.push_back(createInfo.pAttachments[i].pTexture);
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

		bool allSizesValid = true;
		for (u32 i = 0; i < createInfo.attachmentCount; i++)
		{
			FramebufferAttachmentDesc& currAttachment = createInfo.pAttachments[i];
			ITexture* attachmentTex = currAttachment.pTexture;
			if (attachmentTex->GetWidth() != createInfo.width || attachmentTex->GetHeight() != createInfo.height)
			{
				allSizesValid = false;
				LogWarning("Attempting to create framebuffer of size %ux%u, but attachment %u has size %ux%u", 
					createInfo.width, createInfo.height, i, attachmentTex->GetWidth(), attachmentTex->GetHeight());
			}
		}
		if (!allSizesValid)
		{
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