
#include <vulkan/vk_enum_string_helper.h>

#include "render_pass_cache.h"

#include "utils/logger.h"
#include "../render_device_vk.h"
#include "../texture_vk.h"
#include "texture_type_converter.h"

namespace PHX
{
	RenderPassCache::RenderPassCache() : m_cache()
	{
	}

	VkRenderPass RenderPassCache::Find(const RenderPassDescription& desc) const
	{
		auto it = m_cache.find(desc);
		if (it != m_cache.end())
		{
			return it->second;
		}

		return VK_NULL_HANDLE;
	}

	VkRenderPass RenderPassCache::Insert(RenderDeviceVk* pRenderDevice, const RenderPassDescription& desc)
	{
		auto it = m_cache.find(desc);
		if (it != m_cache.end())
		{
			return it->second;
		}

		VkRenderPass pass = CreateFromDescription(pRenderDevice, desc);
		m_cache.insert({desc, pass});

		return pass;
	}

	void RenderPassCache::Delete(const RenderPassDescription& desc)
	{
		auto it = m_cache.find(desc);
		if (it != m_cache.end())
		{
			m_cache.erase(it);
		}
	}

	VkRenderPass RenderPassCache::CreateFromDescription(RenderDeviceVk* pRenderDevice, const RenderPassDescription& desc) const
	{
		std::vector<VkAttachmentDescription> attachmentDescs;
		std::vector<VkAttachmentReference> attachmentRefs;

		attachmentDescs.resize(desc.attachments.size());
		attachmentRefs.resize(desc.attachments.size());
		for (u32 i = 0; i < static_cast<u32>(attachmentDescs.size()); i++)
		{
			const AttachmentDescription& attachmentDesc = desc.attachments.at(i);

			VkAttachmentDescription& attachmentDescVk = attachmentDescs.at(i);
			attachmentDescVk.format = TEX_UTILS::ConvertBaseFormat(attachmentDesc.pTexture->GetFormat());
			attachmentDescVk.samples = TEX_UTILS::ConvertSampleCount(attachmentDesc.pTexture->GetSampleCount());
			attachmentDescVk.loadOp = attachmentDesc.loadOp;
			attachmentDescVk.storeOp = attachmentDesc.storeOp;
			attachmentDescVk.stencilLoadOp = attachmentDesc.stencilLoadOp;
			attachmentDescVk.stencilStoreOp = attachmentDesc.stencilStoreOp;
			attachmentDescVk.initialLayout = attachmentDesc.initialLayout;
			attachmentDescVk.finalLayout = attachmentDesc.finalLayout;

			VkAttachmentReference& attachmentRef = attachmentRefs.at(i);
			attachmentRef.attachment = i;
			attachmentRef.layout = attachmentDesc.layout;
		}

		VkSubpassDescription subpassDescVk{};
		VkSubpassDependency subpassDepVk{};

		if (desc.subpasses.size() > 1)
		{
			LogWarning("Only one subpass is currently supported for render passes!");
		}
		const SubpassDescription& subpassInfo = desc.subpasses.at(0);

		subpassDescVk.pipelineBindPoint = subpassInfo.bindPoint;
		subpassDescVk.colorAttachmentCount = static_cast<u32>(subpassInfo.colorAttachmentIndices.size());

		if (subpassInfo.depthStencilAttachmentIndex != U32_MAX)
		{
			subpassDescVk.pDepthStencilAttachment = &(attachmentRefs.at(subpassInfo.depthStencilAttachmentIndex));
		}

		if (subpassInfo.resolveAttachmentIndex != U32_MAX)
		{
			subpassDescVk.pResolveAttachments = &(attachmentRefs.at(subpassInfo.resolveAttachmentIndex));
		}
			
		std::vector<VkAttachmentReference> colorAttachmentRefs;
		colorAttachmentRefs.reserve(subpassInfo.colorAttachmentIndices.size());
		for (const auto& colorAttachmentIndex : subpassInfo.colorAttachmentIndices)
		{
			colorAttachmentRefs.push_back(attachmentRefs.at(colorAttachmentIndex));
		}
		subpassDescVk.pColorAttachments = colorAttachmentRefs.data();

		// Subpass dependencies
		subpassDepVk.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDepVk.dstSubpass = 0;
		subpassDepVk.srcStageMask = subpassInfo.srcStageMask;
		subpassDepVk.dstStageMask = subpassInfo.dstStageMask;
		subpassDepVk.dstAccessMask = subpassInfo.dstAccessMask;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<u32>(attachmentDescs.size());
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescVk;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &subpassDepVk;

		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkResult res = vkCreateRenderPass(pRenderDevice->GetLogicalDevice(), &renderPassInfo, nullptr, &renderPass);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create render pass from description! Got error: \"%s\"", string_VkResult(res));
		}

		// If the above fails, is renderPass still VK_NULL_HANDLE?
		return renderPass;
	}
}