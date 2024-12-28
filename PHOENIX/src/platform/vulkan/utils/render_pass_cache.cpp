
#include "render_pass_cache.h"

#include "../../../utils/logger.h"
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
			attachmentDescVk.format = TEX_UTILS::ConvertTextureFormat(attachmentDesc.pTexture->GetFormat());
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

		std::vector<VkSubpassDescription> subpassDescs;
		std::vector<VkSubpassDependency> subpassDeps;

		subpassDescs.resize(desc.subpasses.size());
		subpassDeps.resize(desc.subpasses.size());
		for (u32 i = 0; i < static_cast<u32>(subpassDescs.size()); i++)
		{
			const SubpassDescription& subpassInfo = desc.subpasses.at(i);

			VkSubpassDescription& subpassDescVk = subpassDescs.at(i);
			subpassDescVk.pipelineBindPoint = subpassInfo.bindPoint;
			subpassDescVk.colorAttachmentCount = static_cast<u32>(subpassInfo.colorAttachmentIndices.size());
			subpassDescVk.pDepthStencilAttachment = &(attachmentRefs.at(subpassInfo.depthStencilAttachmentIndex));
			subpassDescVk.pResolveAttachments = &(attachmentRefs.at(subpassInfo.resolveAttachmentIndex));
			
			std::vector<VkAttachmentReference> colorAttachmentRefs;
			colorAttachmentRefs.resize(subpassInfo.colorAttachmentIndices.size());
			for (const auto& colorAttachmentIndex : subpassInfo.colorAttachmentIndices)
			{
				colorAttachmentRefs.push_back(attachmentRefs.at(colorAttachmentIndex));
			}
			subpassDescVk.pColorAttachments = colorAttachmentRefs.data();

			VkSubpassDependency& subpassDepVk = subpassDeps.at(i);
			subpassDepVk.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpassDepVk.dstSubpass = 0;
			subpassDepVk.srcStageMask = subpassInfo.srcStageMask;
			subpassDepVk.dstStageMask = subpassInfo.dstStageMask;
			subpassDepVk.dstAccessMask = subpassInfo.dstAccessMask;
		}

		/*VkAttachmentDescription colorAttachmentDesc{};
		colorAttachmentDesc.format = colorAttachmentFormat;
		colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference& colorAttachmentRef = out_builder.GetNextAttachmentReference();
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = nullptr;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;*/

		////////////////////////////////////////////////////////////////////////////

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<u32>(attachmentDescs.size());
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.subpassCount = static_cast<u32>(subpassDescs.size());
		renderPassInfo.pSubpasses = subpassDescs.data();
		renderPassInfo.dependencyCount = static_cast<u32>(subpassDeps.size());
		renderPassInfo.pDependencies = subpassDeps.data();

		VkRenderPass renderPass = VK_NULL_HANDLE;
		if (vkCreateRenderPass(pRenderDevice->GetLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			LogError("Failed to create render pass from description!");
		}

		// If the above fails, is renderPass still VK_NULL_HANDLE?
		return renderPass;
	}
}