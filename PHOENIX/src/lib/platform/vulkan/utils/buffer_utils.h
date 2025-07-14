#pragma once

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "../render_device_vk.h"

namespace PHX
{
	struct BufferData
	{
		bool isValid = false;
		u64 size = 0;
		VkBuffer buffer = VK_NULL_HANDLE;
		VmaAllocation alloc = VK_NULL_HANDLE;
		VmaAllocationInfo allocInfo = {};
	};

	BufferData CreateBuffer(RenderDeviceVk* pRenderDevice, u64 size, VkBufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags);
	void DestroyBuffer(RenderDeviceVk* pRenderDevice, BufferData& buffer);

	bool ShouldUseDirectMemoryMapping(BUFFER_USAGE usage);
}