
#include <vulkan/vk_enum_string_helper.h>

#include "framebuffer_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "render_device_vk.h"
#include "texture_vk.h"
#include "utils/attachment_type_converter.h"

namespace PHX
{
	FramebufferDescription::~FramebufferDescription()
	{
		if (shouldDeleteAttachments)
		{
			SAFE_DEL(pAttachments);
		}
	}

	FramebufferDescription::FramebufferDescription(const FramebufferDescription& other) : 
		width(other.width), height(other.height), layers(other.layers), renderPass(other.renderPass), 
		isBackbuffer(other.isBackbuffer), attachmentCount(other.attachmentCount)
	{
		// Deep copy attachments
		pAttachments = new FramebufferAttachmentDesc[attachmentCount];
		for (u32 i = 0; i < attachmentCount; i++)
		{
			pAttachments[i] = other.pAttachments[i];
		}

		shouldDeleteAttachments = true;
	}

	FramebufferDescription& FramebufferDescription::operator=(const FramebufferDescription& other)
	{
		if (this == &other)
		{
			return *this;
		}

		width = other.width;
		height = other.height;
		layers = other.layers;
		renderPass = other.renderPass;
		isBackbuffer = other.isBackbuffer;
		attachmentCount = other.attachmentCount;

		// Deep copy attachments
		if (pAttachments != nullptr)
		{
			SAFE_DEL(pAttachments);
		}

		pAttachments = new FramebufferAttachmentDesc[attachmentCount];
		for (u32 i = 0; i < attachmentCount; i++)
		{
			pAttachments[i] = other.pAttachments[i];
		}

		shouldDeleteAttachments = true;

		return *this;
	}

	bool FramebufferDescription::operator==(const FramebufferDescription& other) const
	{
		const bool isBaseDataValid = (width == other.width)						&&
									 (height == other.height)					&&
									 (layers == other.layers)					&&
									 (attachmentCount == other.attachmentCount) &&
									 (renderPass == other.renderPass)			&&
									 (isBackbuffer == other.isBackbuffer);

		if (!isBaseDataValid)
		{
			return false;
		}

		bool allAttachmentsEqual = true;
		for (u32 i = 0; i < attachmentCount; i++)
		{
			const FramebufferAttachmentDesc& thisAtt = pAttachments[i];
			const FramebufferAttachmentDesc& otherAtt = other.pAttachments[i];

			allAttachmentsEqual &= (thisAtt.pTexture == otherAtt.pTexture);
			allAttachmentsEqual &= (thisAtt.mipTarget == otherAtt.mipTarget);
			allAttachmentsEqual &= (thisAtt.type == otherAtt.type);
			allAttachmentsEqual &= (thisAtt.loadOp == otherAtt.loadOp);
			allAttachmentsEqual &= (thisAtt.storeOp == otherAtt.storeOp);
		}

		return allAttachmentsEqual;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	FramebufferVk::FramebufferVk(RenderDeviceVk* pRenderDevice, const FramebufferDescription& desc)
	{
		if (VerifyDescription(desc) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		std::vector<VkImageView> imageViews;
		imageViews.reserve(desc.attachmentCount);

		for (u32 i = 0; i < desc.attachmentCount; i++)
		{
			FramebufferAttachmentDesc& attachmentDesc = desc.pAttachments[i];
			u32 mipTarget = attachmentDesc.mipTarget;
			VkImageView imageView = (attachmentDesc.pTexture)->GetImageViewAt(mipTarget);
			if (imageView == VK_NULL_HANDLE)
			{
				LogWarning("Mip target at index %u doesn't exist! Skipping target during framebuffer creation", i);
				continue;
			}

			imageViews.push_back(imageView);
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = desc.renderPass;
		framebufferInfo.attachmentCount = desc.attachmentCount;
		framebufferInfo.pAttachments = imageViews.data();
		framebufferInfo.width = desc.width;
		framebufferInfo.height = desc.height;
		framebufferInfo.layers = desc.layers;
		VkResult res = vkCreateFramebuffer(pRenderDevice->GetLogicalDevice(), &framebufferInfo, nullptr, &m_framebuffer);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create framebuffer! Got error: %s", string_VkResult(res));
			return;
		}

		// Cache the attachments
		m_attachments.reserve(desc.attachmentCount);
		for (u32 i = 0; i < desc.attachmentCount; i++)
		{
			m_attachments.push_back(desc.pAttachments[i].pTexture);
		}
		m_width = desc.width;
		m_height = desc.height;
		m_layers = desc.layers;
		m_renderPass = desc.renderPass;

		// Log creation info
		LogDebug("FRAMEBUFFER CREATED: width (%u), height (%u), layers (%u), attachments (%u), isBackbuffer (%s)", m_width, m_height, m_layers, desc.attachmentCount, desc.isBackbuffer ? "true" : "false");
		for (u32 i = 0; i < desc.attachmentCount; i++)
		{
			const FramebufferAttachmentDesc& attachmentDesc = desc.pAttachments[i];

			LogDebug("\tAttachment %u", i);
			LogDebug("\t- Texture ptr: %p", attachmentDesc.pTexture);
			LogDebug("\t- Mip target:  %u", attachmentDesc.mipTarget);
			LogDebug("\t- Load op:     %s", string_VkAttachmentLoadOp(ATT_UTILS::ConvertLoadOp(attachmentDesc.loadOp)));
			LogDebug("\t- Store op:    %s", string_VkAttachmentStoreOp(ATT_UTILS::ConvertStoreOp(attachmentDesc.storeOp)));
		}

		m_pRenderDevice = pRenderDevice;
	}

	FramebufferVk::~FramebufferVk()
	{
		vkDestroyFramebuffer(m_pRenderDevice->GetLogicalDevice(), m_framebuffer, nullptr);

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

	TextureVk* FramebufferVk::GetAttachment(u32 index)
	{
		if (index >= m_attachments.size())
		{
			return nullptr;
		}

		return m_attachments.at(index);
	}

	VkRenderPass FramebufferVk::GetRenderPass() const
	{
		return m_renderPass;
	}

	VkFramebuffer FramebufferVk::GetFramebuffer() const
	{
		return m_framebuffer;
	}

	STATUS_CODE FramebufferVk::VerifyDescription(const FramebufferDescription& desc)
	{
		if (desc.attachmentCount == 0)
		{
			LogError("Attempting to create a framebuffer with no attachments!");
			return STATUS_CODE::ERR_API;
		}

		if (desc.width == 0 || desc.height == 0 || desc.layers == 0)
		{
			LogError("Attempting to create a framebuffer with no width, height or layer count!");
			return STATUS_CODE::ERR_API;
		}

		if (desc.pAttachments == nullptr || desc.attachmentCount == 0)
		{
			LogError("Attempting to create a framebuffer with no attachments!");
			return STATUS_CODE::ERR_API;
		}

		bool allSizesValid = true;
		for (u32 i = 0; i < desc.attachmentCount; i++)
		{
			FramebufferAttachmentDesc& currAttachment = desc.pAttachments[i];
			ITexture* attachmentTex = currAttachment.pTexture;
			if (attachmentTex->GetWidth() != desc.width || attachmentTex->GetHeight() != desc.height)
			{
				allSizesValid = false;
				LogWarning("Attempting to create framebuffer of size %ux%u, but attachment %u has size %ux%u", 
					desc.width, desc.height, i, attachmentTex->GetWidth(), attachmentTex->GetHeight());
			}
		}
		if (!allSizesValid)
		{
			return STATUS_CODE::ERR_API;
		}

		//if (createInfo.renderPass == nullptr)
		//{
		//	LogError("Attempting to create framebuffer with an invalid render pass. Render pass is null");
		//	return;
		//}

		return STATUS_CODE::SUCCESS;
	}

}