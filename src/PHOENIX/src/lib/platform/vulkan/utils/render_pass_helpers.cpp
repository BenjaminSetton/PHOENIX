
#include "render_pass_helpers.h"

#include "BSL/integral_types.h"
#include "BSL/sanity.h"
#include "utils/cache_utils.h"

namespace PHX
{
	static bool AreAttachmentsEqual(const AttachmentDescription& A, const AttachmentDescription& B)
	{
		// Ignore texture pointers for description comparisons
		return (A.loadOp          == B.loadOp          &&
				A.storeOp         == B.storeOp         &&
				A.stencilLoadOp   == B.stencilLoadOp   &&
				A.stencilStoreOp  == B.stencilStoreOp  &&
				A.initialLayout   == B.initialLayout   &&
				A.finalLayout     == B.finalLayout     &&
				A.layout          == B.layout);
	}

	static bool AreSubpassesEqual(const SubpassDescription& A, const SubpassDescription& B)
	{
		return (A.bindPoint                    == B.bindPoint                    &&
				A.colorAttachmentIndices       == B.colorAttachmentIndices       &&
				A.depthStencilAttachmentIndex  == B.depthStencilAttachmentIndex  &&
				A.resolveAttachmentIndex       == B.resolveAttachmentIndex       &&
				A.srcStageMask                 == B.srcStageMask                 &&
				A.dstStageMask                 == B.dstStageMask                 &&
				A.srcAccessMask                == B.srcAccessMask                &&
				A.dstAccessMask                == B.dstAccessMask);
	}

	bool RenderPassDescription::operator==(const RenderPassDescription& other) const
	{
		STATIC_ASSERT_MSG(sizeof(AttachmentDescription) == 40, "If attachment description changed, make sure to change this equality function!");

		if (attachments.size() != other.attachments.size())
		{
			return false;
		}

		for (u32 i = 0; i < attachments.size(); i++)
		{
			if (!AreAttachmentsEqual(attachments[i], other.attachments[i]))
			{
				return false;
			}
		}

		if (subpasses.size() != other.subpasses.size())
		{
			return false;
		}

		for (u32 i = 0; i < subpasses.size(); i++)
		{
			if (!AreSubpassesEqual(subpasses[i], other.subpasses[i]))
			{
				return false;
			}
		}

		return true;
	}

	size_t RenderPassDescriptionHasher::operator()(const RenderPassDescription& desc) const
	{
		STATIC_ASSERT_MSG(sizeof(AttachmentDescription) == 40, "If attachment description changed, make sure to change this hashing function!");

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
			HashCombine(seed, attachment.layout);
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
			HashCombine(seed, subpass.srcAccessMask);
			HashCombine(seed, subpass.dstAccessMask);
		}

		return seed;
	}
}