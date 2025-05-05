
#include "framebuffer_cache.h"

#include "../render_device_vk.h"
#include "utils/cache_utils.h"

namespace PHX
{
	size_t FramebufferDescriptionHasher::operator()(const FramebufferDescription& desc) const
	{
		//STATIC_ASSERT_MSG(sizeof(desc) == SOME_SIZE, "If framebuffer description changed, make sure to change this hashing function!");

		size_t seed = 0;

		HashCombine(seed, desc.width);
		HashCombine(seed, desc.height);
		HashCombine(seed, desc.layers);
		HashCombine(seed, desc.attachmentCount);
		for (u32 i = 0; i < desc.attachmentCount; i++)
		{
			const FramebufferAttachmentDesc& currAtt = desc.pAttachments[i];
			HashCombine(seed, currAtt.mipTarget);
			HashCombine(seed, currAtt.type);
			HashCombine(seed, currAtt.loadOp);
			HashCombine(seed, currAtt.storeOp);
		}

		return seed;
	}

	FramebufferVk* FramebufferCache::Find(const FramebufferDescription& desc) const
	{
		auto iter = m_cache.find(desc);
		if (iter == m_cache.end())
		{
			return nullptr;
		}

		return iter->second;
	}

	FramebufferVk* FramebufferCache::FindOrCreate(RenderDeviceVk* pRenderDevice, const FramebufferDescription& desc)
	{
		FramebufferVk* res = nullptr;

		auto iter = m_cache.find(desc);
		if (iter == m_cache.end())
		{
			FramebufferVk* pFramebuffer = new FramebufferVk(pRenderDevice, desc);
			m_cache.insert({ desc, pFramebuffer });
			res = pFramebuffer;
		}
		else
		{
			res = iter->second;
		}

		return res;
	}

	void FramebufferCache::Delete(const FramebufferDescription& desc)
	{
		auto iter = m_cache.find(desc);
		if (iter != m_cache.end())
		{
			m_cache.erase(iter);
		}
	}
}