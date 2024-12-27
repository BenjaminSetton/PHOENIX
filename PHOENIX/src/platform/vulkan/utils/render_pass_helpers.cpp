
#include "render_pass_helpers.h"

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Thanks ChatGPT :)
	template <typename T>
	inline void hash_combine(size_t& seed, const T& value) 
	{
		seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	bool RenderPassDescription::operator==(const RenderPassDescription& other) const
	{
		if (attachments.size() != other.attachments.size())
		{
			return false;
		}

		bool isEqual = true;
		for (u32 i = 0; i < attachments.size(); i++)
		{
			const AttachmentDescription& thisAttachment = attachments.at(i);
			const AttachmentDescription& otherAttachment = other.attachments.at(i);
			if (memcmp(&thisAttachment, &otherAttachment, sizeof(AttachmentDescription)) != 0)
			{
				isEqual = false;
				break;
			}
		}

		return isEqual;
	}

	size_t RenderPassDescriptionHasher::operator()(const RenderPassDescription& desc) const
	{
		size_t seed = 0;

		hash_combine(seed, desc.attachments.size());
		for (const auto& attachment : desc.attachments)
		{
			hash_combine(seed, attachment.pTexture);
			hash_combine(seed, attachment.loadOp);
			hash_combine(seed, attachment.storeOp);
			hash_combine(seed, attachment.stencilLoadOp);
			hash_combine(seed, attachment.stencilStoreOp);
			hash_combine(seed, attachment.initialLayout);
			hash_combine(seed, attachment.finalLayout);
		}

		hash_combine(seed, desc.subpasses.size());
		for (const auto& subpass : desc.subpasses)
		{
			hash_combine(seed, subpass.bindPoint);
			hash_combine(seed, subpass.colorAttachmentIndices.size());
			hash_combine(seed, subpass.depthStencilAttachmentIndex);
			hash_combine(seed, subpass.resolveAttachmentIndex);
			hash_combine(seed, subpass.srcStageMask);
			hash_combine(seed, subpass.dstStageMask);
			hash_combine(seed, subpass.dstAccessMask);
		}

		return seed;
	}
}