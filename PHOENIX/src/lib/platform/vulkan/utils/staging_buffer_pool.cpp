
#include <string>

#include "staging_buffer_pool.h"

#include "buffer_utils.h"
#include "utils/logger.h"
#include "utils/math.h"

namespace PHX
{
	// TODO - Expose to client
	static constexpr u64 DEFAULT_POOL_SIZE = MB(32);

	StagingBufferPool::StagingBufferPool(RenderDeviceVk* pRenderDevice) :
	 	m_renderDevice(nullptr), m_poolBuffers(), m_poolOffsets()
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to create staging buffer pool. Render device is null!");
			return;
		}
		m_renderDevice = pRenderDevice;
	}

	StagingBufferPool::~StagingBufferPool()
	{
		Destroy();
	}

	StagingAllocation StagingBufferPool::Allocate(u64 sizeBytes, u64 alignment)
	{
		StagingAllocation alloc{};

		if (m_renderDevice == nullptr)
		{
			LogError("Failed to allocate from staging pool. Render device is null!");
			return alloc;
		}

		if (sizeBytes == 0 || alignment == 0)
		{
			LogWarning("Skipped staging pool allocation. Size or alignment is 0!");
			return alloc;
		}

		// Search existing pools for one with enough remaining space
		for (u32 i = 0; i < m_poolBuffers.size(); i++)
		{
			const BufferData& currPoolBuffer = m_poolBuffers[i];
			u64 alignedOffset = AlignUp(m_poolOffsets[i], alignment);
			if (alignedOffset + sizeBytes <= currPoolBuffer.size)
			{
				alloc.buffer = currPoolBuffer.buffer;
				alloc.offset = alignedOffset;
				alloc.mappedData = static_cast<u8*>(currPoolBuffer.allocInfo.pMappedData) + alignedOffset;
				alloc.isValid = true;
				m_poolOffsets[i] = alignedOffset + sizeBytes; // Update pool offset to reflect new allocation
				return alloc;
			}
		}

		// No existing pool has enough space, allocate a new one
		u64 newPoolSize = DEFAULT_POOL_SIZE;
		if (sizeBytes > newPoolSize)
		{
			newPoolSize = AlignUp(sizeBytes, DEFAULT_POOL_SIZE);
		}

		const VmaAllocationCreateFlags poolFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		const VkBufferUsageFlags poolUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		std::string poolName = "StagingBufferPool_" + std::to_string(m_poolBuffers.size());
		BufferData newPool = CreateBuffer(m_renderDevice, poolName.c_str(), newPoolSize, poolUsage, poolFlags, 0, 0);
		if (!newPool.isValid)
		{
			LogError("Failed to create staging buffer pool of size %llu bytes!", newPoolSize);
			return alloc;
		}

		LogDebug("Created new staging buffer pool of size %llu bytes!", newPoolSize);

		m_poolBuffers.push_back(newPool);
		m_poolOffsets.push_back(sizeBytes);

		alloc.buffer = newPool.buffer;
		alloc.offset = 0;
		alloc.mappedData = newPool.allocInfo.pMappedData;
		alloc.isValid = true;

		return alloc;
	}

	void StagingBufferPool::Reset()
	{
		for (u64& offset : m_poolOffsets)
		{
			offset = 0;
		}
	}

	void StagingBufferPool::Destroy()
	{
		if (m_renderDevice == nullptr)
		{
			return;
		}

		for (BufferData& pool : m_poolBuffers)
		{
			DestroyBuffer(m_renderDevice, pool);
		}
		m_poolBuffers.clear();
		m_poolOffsets.clear();
	}
}