
#include <vma/vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include "buffer_vk.h"
#include "utils/logger.h"
#include "utils/buffer_type_converter.h"

namespace PHX
{
	static constexpr u32 MIN_SIZE_FOR_DEDICATED_MEMORY = 128; // in bytes

	BufferVk::BufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo) : m_renderDevice(VK_NULL_HANDLE), m_usage()
	{
		if (createInfo.sizeBytes == 0)
		{
			LogError("Failed to create buffer. Buffer size is 0!");
			return;
		}
		m_renderDevice = pRenderDevice;

		BufferData newBuffer{};

		// Create buffer
		VmaAllocationCreateFlags bufferCreateFlags = 0;
		if (createInfo.sizeBytes >= MIN_SIZE_FOR_DEDICATED_MEMORY)
		{
			// Create dedicated memory for "large" allocations, such as larger buffers or images
			bufferCreateFlags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		}
		if (createInfo.bufferUsage == BUFFER_USAGE::UNIFORM_BUFFER)
		{
			// Use create mapped flag for uniform buffers, plus other required flags
			bufferCreateFlags |= (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		}

		const VkBufferUsageFlags bufferUsageFlags = BUFFER_UTILS::ConvertBufferUsage(createInfo.bufferUsage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		newBuffer = CreateBuffer(createInfo.sizeBytes, bufferUsageFlags, bufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create buffer!");
			return;
		}
		m_buffer = newBuffer;

		// Create staging buffer, if applicable
		if (createInfo.bufferUsage != BUFFER_USAGE::UNIFORM_BUFFER)
		{
			const VmaAllocationCreateFlags stagingBufferCreateFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			const VkBufferUsageFlags stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			newBuffer = CreateBuffer(createInfo.sizeBytes, stagingBufferUsageFlags, stagingBufferCreateFlags, 0, 0);
			if (!newBuffer.isValid)
			{
				LogError("Failed to create staging buffer!");
				DestroyBuffer(m_buffer); // Clean up the buffer object which previously succeeded
				return;
			}
			m_stagingBuffer = newBuffer;
		}

		m_usage = createInfo.bufferUsage;
	}

	BufferVk::~BufferVk()
	{
		DestroyBuffer(m_stagingBuffer);
		DestroyBuffer(m_buffer);
	}

	BUFFER_USAGE BufferVk::GetUsage() const
	{
		return m_usage;
	}

	VkDeviceSize BufferVk::GetSize() const
	{
		return m_buffer.size;
	}

	STATUS_CODE BufferVk::CopyData(const void* data, u64 size)
	{
		if (!m_stagingBuffer.isValid && !m_buffer.isValid)
		{
			LogError("Failed to copy data to staging buffer! Both the buffer and staging buffer are invalid");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VmaAllocation destAllocation = VK_NULL_HANDLE;
		if (m_usage == BUFFER_USAGE::UNIFORM_BUFFER)
		{
			destAllocation = m_buffer.alloc;
		}
		else
		{
			destAllocation = m_stagingBuffer.alloc;
		}

		VkResult res = vmaCopyMemoryToAllocation(m_renderDevice->GetAllocator(), data, destAllocation, 0, size);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to copy data to buffer! Got result: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
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

	VkDeviceSize BufferVk::GetAllocatedSize() const
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
		newData.size = size;

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