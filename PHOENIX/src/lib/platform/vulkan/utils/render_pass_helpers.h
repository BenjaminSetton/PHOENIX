#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Forward declarations
	class TextureVk;

	struct AttachmentDescription
	{
		TextureVk* pTexture					= nullptr;
		VkAttachmentLoadOp loadOp			= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VkAttachmentStoreOp storeOp			= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkAttachmentLoadOp stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		VkAttachmentStoreOp stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
		VkImageLayout initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout finalLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
		VkImageLayout layout				= VK_IMAGE_LAYOUT_UNDEFINED;
	};

	struct SubpassDescription
	{
		VkPipelineBindPoint bindPoint				= VK_PIPELINE_BIND_POINT_GRAPHICS;
		std::vector<u32> colorAttachmentIndices;
		u32 depthStencilAttachmentIndex				= U32_MAX;
		u32 resolveAttachmentIndex					= U32_MAX;
		VkPipelineStageFlags srcStageMask			= VK_PIPELINE_STAGE_NONE;
		VkPipelineStageFlags dstStageMask			= VK_PIPELINE_STAGE_NONE;
		VkAccessFlags srcAccessMask					= VK_ACCESS_NONE;
		VkAccessFlags dstAccessMask					= VK_ACCESS_NONE;
	};

	// Provides a description of a render pass. This object is hashed
	// and used in the render pass cache
	struct RenderPassDescription
	{
		std::vector<AttachmentDescription> attachments;
		std::vector<SubpassDescription> subpasses;

		///////
		bool operator==(const RenderPassDescription& other) const;
		///////
	};

	// Used to hash custom key type for cache
	struct RenderPassDescriptionHasher
	{
		size_t operator()(const RenderPassDescription& desc) const;
	};
}