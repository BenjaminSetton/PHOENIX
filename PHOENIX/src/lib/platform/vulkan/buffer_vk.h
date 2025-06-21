#pragma once

#include "PHX/interface/buffer.h"

#include "render_device_vk.h"

namespace PHX
{
	struct BufferData
	{
		bool isValid                = false;
		u64 size                    = 0;
		VkBuffer buffer             = VK_NULL_HANDLE;
		VmaAllocation alloc         = VK_NULL_HANDLE;
		VmaAllocationInfo allocInfo = {};
	};

	class BufferVk : public IBuffer
	{
	public:

		BufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo);
		~BufferVk();

		STATUS_CODE CopyData(const void* data, u64 size) override;
		BUFFER_USAGE GetUsage() const override;

		VkBuffer GetBuffer() const;
		VkBuffer GetStagingBuffer() const;
		VkDeviceSize GetOffset() const;
		u64 GetSize() const;
		u64 GetAllocatedSize() const; // May differ from GetSize() because of alignment

	private:

		BufferData CreateBuffer(u64 size, VkBufferUsageFlags usageFlags, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags, VkMemoryPropertyFlags preferredFlags);
		void DestroyBuffer(BufferData& buffer);

	private:

		RenderDeviceVk* m_renderDevice;

		BufferData m_buffer;
		BufferData m_stagingBuffer;
		BUFFER_USAGE m_usage;

	};
}