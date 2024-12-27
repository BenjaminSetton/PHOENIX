#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace PHX
{
	// Forward declarations
	class TextureVk;

	struct AttachmentDescription
	{
		TextureVk* pTexture;
		VkAttachmentLoadOp loadOp;
		VkAttachmentStoreOp storeOp;
		VkAttachmentLoadOp stencilLoadOp;
		VkAttachmentStoreOp stencilStoreOp;
		VkImageLayout initialLayout;
		VkImageLayout finalLayout;
	};

	struct SubpassDescription
	{
		VkPipelineBindPoint bindPoint;
		std::vector<u32> colorAttachmentIndices;
		u32 depthStencilAttachmentIndex;
		u32 resolveAttachmentIndex;
		VkPipelineStageFlags srcStageMask;
		VkPipelineStageFlags dstStageMask;
		VkAccessFlags dstAccessMask;
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