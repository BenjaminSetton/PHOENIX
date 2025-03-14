
#include <vma/vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include "buffer_vk.h"
#include "utils/logger.h"
#include "utils/buffer_type_converter.h"

namespace PHX
{
	BufferVk::BufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo)
	{
		m_renderDevice = pRenderDevice;

		BufferData newBuffer{};

		// Create buffer
		const VmaAllocationCreateFlags bufferCreateFlags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		const VkBufferUsageFlags bufferUsageFlags = BUFFER_UTILS::ConvertBufferUsage(createInfo.bufferUsage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		newBuffer = CreateBuffer(createInfo.size, bufferUsageFlags, bufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create buffer!");
			return;
		}
		m_buffer = newBuffer;

		// Create staging buffer
		const VmaAllocationCreateFlags stagingBufferCreateFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		const VkBufferUsageFlags stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		newBuffer = CreateBuffer(createInfo.size, stagingBufferUsageFlags, stagingBufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create staging buffer!");
			DestroyBuffer(m_buffer); // Clean up the buffer object which previously succeeded
			return;
		}
		m_stagingBuffer = newBuffer;
	}

	BufferVk::~BufferVk()
	{
		DestroyBuffer(m_stagingBuffer);
		DestroyBuffer(m_buffer);
	}

	STATUS_CODE BufferVk::CopyDataToStagingBuffer(const void* data, u64 size)
	{
		if (!m_stagingBuffer.isValid || !m_buffer.isValid)
		{
			LogError("Failed to copy data to staging buffer! Either the buffer, staging buffer, or both are invalid");
			return STATUS_CODE::ERR;
		}

		VkResult res = vmaCopyMemoryToAllocation(m_renderDevice->GetAllocator(), data, m_stagingBuffer.alloc, 0, size);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to copy data to staging buffer! Got result: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;

		//vmaMapMemory(_allocator, stagingAllocation, &data);
		//memcpy(data, bufferData, bufferSize);
		//vmaUnmapMemory(_allocator, stagingAllocation);
	}

	VkBuffer BufferVk::GetBuffer() const
	{
		return m_buffer.buffer;
	}

	VkBuffer BufferVk::GetStagingBuffer() const
	{
		return m_stagingBuffer.buffer;
	}

	VkDeviceSize BufferVk::GetOffset() const
	{
		return m_buffer.allocInfo.offset;
	}

	VkDeviceSize BufferVk::GetSize() const
	{
		return m_buffer.allocInfo.size;
	}

	BufferData BufferVk::CreateBuffer(u64 size, VkBufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags)
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

		VkResult res = vmaCreateBuffer(m_renderDevice->GetAllocator(), &vkBufferInfo, &vmaAllocInfo, &newData.buffer, &newData.alloc, &newData.allocInfo);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to allocate buffer! Got result: %s", string_VkResult(res));
			newData.isValid = false;
		}

		return newData;
	}

	void BufferVk::DestroyBuffer(BufferData& buffer)
	{
		if (buffer.isValid)
		{
			vmaDestroyBuffer(m_renderDevice->GetAllocator(), buffer.buffer, buffer.alloc);
		}
	}
}