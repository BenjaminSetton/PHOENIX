
#include <vulkan/vk_enum_string_helper.h>

#include "staging_buffer.h"

#include "buffer_utils.h"
#include "utils/logger.h"

namespace PHX
{
	StagingBufferVk::StagingBufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo) : m_renderDevice(nullptr), m_buffer()
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to create staging buffer. Render device is null!");
			return;
		}
		m_renderDevice = pRenderDevice;

		const VmaAllocationCreateFlags stagingBufferCreateFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		const VkBufferUsageFlags stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		BufferData newBuffer = CreateBuffer(m_renderDevice, createInfo.sizeBytes, stagingBufferUsageFlags, stagingBufferCreateFlags, 0, 0);
		if (!newBuffer.isValid)
		{
			LogError("Failed to create staging buffer!");
			return;
		}
		m_buffer = newBuffer;
	}

	StagingBufferVk::~StagingBufferVk()
	{
		DestroyBuffer(m_renderDevice, m_buffer);
	}

	STATUS_CODE StagingBufferVk::CopyData(const void* data, u64 sizeBytes)
	{
		if (!m_buffer.isValid)
		{
			LogError("Failed to copy data to staging buffer! Buffer is invalid");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkResult res = vmaCopyMemoryToAllocation(m_renderDevice->GetAllocator(), data, m_buffer.alloc, 0, sizeBytes);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to copy data to staging buffer! Got result: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	VkBuffer StagingBufferVk::GetBuffer() const
	{
		return m_buffer.buffer;
	}

	bool StagingBufferVk::IsValid() const
	{
		return m_buffer.isValid;
	}
}