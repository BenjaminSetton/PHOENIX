
#include <vma/vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include "BSL/logger.h"
#include "buffer_vk.h"
#include "utils/buffer_type_converter.h"

using namespace BSL;

namespace PHX
{
	static constexpr u32 MIN_SIZE_FOR_DEDICATED_MEMORY = 128; // in bytes

	BufferVk::BufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo) : m_renderDevice(VK_NULL_HANDLE), m_pName(""), m_usage()
	{
		if (createInfo.sizeBytes == 0)
		{
			LogError("Failed to create buffer. Buffer size is 0!");
			return;
		}

		// Warn about mutually exclusive flags being set in createInfo
		HasConflictingUsageFlags(createInfo.bufferUsage);

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

		// TRANSFER_DST is added onto every buffer because CopyDataToBuffer uses a staging
		// buffer + vkCmdCopyBuffer for all non-uniform buffers. This is harmless on buffers that 
		// are never copied to (the driver ignores unused usage flags). TRANSFER_SRC is NOT added,
		// no current API copies from a user buffer to elsewhere. If this is implemented in the future
		// it will be handled then
		const VkBufferUsageFlags bufferUsageFlags = BUFFER_UTILS::ConvertBufferUsageFlags(createInfo.bufferUsage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		newBuffer = CreateBuffer(m_renderDevice, createInfo.pName, createInfo.sizeBytes, bufferUsageFlags, bufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create buffer!");
			return;
		}

		m_pName = createInfo.pName;
		m_buffer = newBuffer;
		m_usage = createInfo.bufferUsage;
	}

	BufferVk::~BufferVk()
	{
		DestroyBuffer(m_renderDevice, m_buffer);
	}

	const char* BufferVk::GetName() const
	{
		return m_pName;
	}

	BufferUsageFlags BufferVk::GetUsage() const
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
			LogWarning("Skipped copying to mapped buffer memory. Buffer is not mapped!");
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
		// Each buffer is created with its own vmaCreateBuffer call, so the offset within
		// the VkBuffer is always 0. The allocInfo.offset is the offset within the underlying
		// VkDeviceMemory block, which is not relevant for Vulkan buffer commands.
		return 0;
	}

	VkDeviceSize BufferVk::GetAllocatedSize() const
	{
		return m_buffer.allocInfo.size;
	}

	bool BufferVk::IsValid() const
	{
		return m_buffer.isValid;
	}

	bool BufferVk::HasConflictingUsageFlags(BufferUsageFlags flags)
	{
		// Vertex and index buffers
		if ((flags & BUFFER_USAGE_FLAG_VERTEX_BUFFER) && (flags & BUFFER_USAGE_FLAG_INDEX_BUFFER))
		{
			LogWarning("Buffer has both VERTEX_BUFFER and INDEX_BUFFER usage flags. These are mutually exclusive.");
			return true;
		}

		// Uniform and storage buffers
		if ((flags & BUFFER_USAGE_FLAG_UNIFORM_BUFFER) && (flags & BUFFER_USAGE_FLAG_STORAGE_BUFFER))
		{
			LogWarning("Buffer has both UNIFORM_BUFFER and STORAGE_BUFFER usage flags. These are mutually exclusive.");
			return true;
		}

		// Acceleration structure storage is dedicated use
		if ((flags & BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE) && (flags & ~BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE))
		{
			LogWarning("Buffer has ACCELERATION_STRUCTURE usage combined with other usage flags. AS storage buffers should be dedicated.");
			return true;
		}

		return false;
	}
}