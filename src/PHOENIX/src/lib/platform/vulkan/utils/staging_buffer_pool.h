#pragma once

#include "BSL/integral_types.h"

#include "../render_device_vk.h"
#include "buffer_utils.h"

namespace PHX
{
	struct StagingAllocation
	{
		VkBuffer buffer 	= VK_NULL_HANDLE;
		u64 offset 			= 0;
		void* mappedData 	= nullptr;
		bool isValid 		= false;
	};

	// Staging buffer pool that sub-allocates from a small number of large VMA allocations.
	// This avoids creating thousands of individual staging buffers,
	// which can overwhelm the driver and cause VK_ERROR_DEVICE_LOST
	class StagingBufferPool
	{
	public:

		explicit StagingBufferPool(RenderDeviceVk* pRenderDevice);
		~StagingBufferPool();

		StagingBufferPool(const StagingBufferPool& other) = delete;
		StagingBufferPool& operator=(const StagingBufferPool& other) = delete;

		// Sub-allocate from the pool. Alignment is applied to the offset.
		// If the current pool buffer is too small, a new one is allocated
		StagingAllocation Allocate(u64 sizeBytes, u64 alignment = 16);

		// Reset the offset to reuse pool memory
		void Reset();

		// Free all pool buffers
		void Destroy();

	private:

		RenderDeviceVk* m_renderDevice;
		std::vector<BufferData> m_poolBuffers;
		std::vector<u64> m_poolOffsets;
	};
}