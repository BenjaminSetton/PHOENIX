
#include "render_pass_helpers.h"

#include "PHX/types/integral_types.h"
#include "utils/cache_utils.h"

namespace PHX
{
	bool RenderPassDescription::operator==(const RenderPassDescription& other) const
	{
		if (attachments.size() != other.attachments.size())
		{
			return false;
		}

		bool isEqual = true;
		for (u32 i = 0; i < attachments.size(); i++)
		{
			AttachmentDescription thisAttachment = attachments[i];
			AttachmentDescription otherAttachment = other.attachments[i];

			// Ignore texture pointers for description comparisons
			thisAttachment.pTexture = nullptr;
			otherAttachment.pTexture = nullptr;

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
		//STATIC_ASSERT_MSG(sizeof(desc) == SOME_SIZE, "If description changed, make sure to change this hashing function!");

		size_t seed = 0;

		HashCombine(seed, desc.attachments.size());
		for (const auto& attachment : desc.attachments)
		{
			// Exclude the texture pointer from the hash because two render pass
			// descriptions can be identical even if their textures are different
			//HashCombine(seed, attachment.pTexture);

			HashCombine(seed, attachment.loadOp);
			HashCombine(seed, attachment.storeOp);
			HashCombine(seed, attachment.stencilLoadOp);
			HashCombine(seed, attachment.stencilStoreOp);
			HashCombine(seed, attachment.initialLayout);
			HashCombine(seed, attachment.finalLayout);
		}

		HashCombine(seed, desc.subpasses.size());
		for (const auto& subpass : desc.subpasses)
		{
			HashCombine(seed, subpass.bindPoint);
			HashCombine(seed, subpass.colorAttachmentIndices.size());
			HashCombine(seed, subpass.depthStencilAttachmentIndex);
			HashCombine(seed, subpass.resolveAttachmentIndex);
			HashCombine(seed, subpass.srcStageMask);
			HashCombine(seed, subpass.dstStageMask);
			HashCombine(seed, subpass.dstAccessMask);
		}

		return seed;
	}
}