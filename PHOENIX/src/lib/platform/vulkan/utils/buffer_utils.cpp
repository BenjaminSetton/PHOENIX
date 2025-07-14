#pragma once

#include <vulkan/vk_enum_string_helper.h>

#include "buffer_utils.h"

#include "utils/logger.h"

namespace PHX
{
	BufferData CreateBuffer(RenderDeviceVk* pRenderDevice, u64 size, VkBufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags)
	{
		VkBufferCreateInfo vkBufferInfo{};
		vkBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vkBufferInfo.size = size;
		vkBufferInfo.usage = usageFlags;
		vkBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Simply using exclusive sharing mode for now

		// SEQUENTIAL_WRITE and USAGE_AUTO flags must go together, per VMA notes
		VmaAllocationCreateInfo vmaAllocInfo{};
		vmaAllocInfo.flags = allocFlags;
		vmaAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		vmaAllocInfo.memoryTypeBits = 0; // No idea what this is
		vmaAllocInfo.pool = VK_NULL_HANDLE;
		vmaAllocInfo.priority = 1.0f; // Max priority?
		vmaAllocInfo.requiredFlags = requiredFlags;
		vmaAllocInfo.preferredFlags = preferredFlags;

		BufferData newData{};
		newData.isValid = true;
		newData.size = size;

		VkResult res = vmaCreateBuffer(pRenderDevice->GetAllocator(), &vkBufferInfo, &vmaAllocInfo, &newData.buffer, &newData.alloc, &newData.allocInfo);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to allocate buffer! Got result: %s", string_VkResult(res));
			newData.isValid = false;
		}

		return newData;
	}

	void DestroyBuffer(RenderDeviceVk* pRenderDevice, BufferData& buffer)
	{
		if (buffer.isValid)
		{
			vmaDestroyBuffer(pRenderDevice->GetAllocator(), buffer.buffer, buffer.alloc);
		}
	}

	bool ShouldUseDirectMemoryMapping(BUFFER_USAGE usage)
	{
		// Only uniform buffers are directly mapped
		return (usage == BUFFER_USAGE::UNIFORM_BUFFER);
	}
}