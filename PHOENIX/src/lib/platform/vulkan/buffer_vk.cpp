
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
		if (ShouldUseDirectMemoryMapping(createInfo.bufferUsage))
		{
			bufferCreateFlags |= (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		}

		const VkBufferUsageFlags bufferUsageFlags = BUFFER_UTILS::ConvertBufferUsage(createInfo.bufferUsage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		newBuffer = CreateBuffer(m_renderDevice, createInfo.sizeBytes, bufferUsageFlags, bufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create buffer!");
			return;
		}

		m_buffer = newBuffer;
		m_usage = createInfo.bufferUsage;
	}

	BufferVk::~BufferVk()
	{
		DestroyBuffer(m_renderDevice, m_buffer);
	}

	BUFFER_USAGE BufferVk::GetUsage() const
	{
		return m_usage;
	}

	VkDeviceSize BufferVk::GetSize() const
	{
		return m_buffer.size;
	}

	STATUS_CODE BufferVk::CopyToMappedData(const void* data, u64 sizeBytes)
	{
		if (!m_buffer.isValid)
		{
			LogError("Failed to copy data to buffer! Buffer is invalid");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Nothing to do if the usage type is not directly mapped. In that case the copy must
		// go through a staging buffer instead
		if (!ShouldUseDirectMemoryMapping(m_usage))
		{
			return STATUS_CODE::SUCCESS;
		}

		VkResult res = vmaCopyMemoryToAllocation(m_renderDevice->GetAllocator(), data, m_buffer.alloc, 0, sizeBytes);
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

	VkDeviceSize BufferVk::GetOffset() const
	{
		return m_buffer.allocInfo.offset;
	}

	VkDeviceSize BufferVk::GetAllocatedSize() const
	{
		return m_buffer.allocInfo.size;
	}

	bool BufferVk::IsValid() const
	{
		return m_buffer.isValid;
	}
}