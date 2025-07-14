#pragma once

#include "PHX/interface/buffer.h"

#include "render_device_vk.h"
#include "utils/buffer_utils.h"

namespace PHX
{
	class BufferVk : public IBuffer
	{
	public:

		explicit BufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo);
		~BufferVk();

		BUFFER_USAGE GetUsage() const override;
		u64 GetSize() const override;

		// Copies to mapped data only. If the buffer's data is not directly mapped
		// this function will do nothing
		STATUS_CODE CopyToMappedData(const void* data, u64 sizeBytes);

		VkBuffer GetBuffer() const;
		VkDeviceSize GetOffset() const;
		u64 GetAllocatedSize() const; // May differ from GetSize() because of alignment
		bool IsValid() const;

	private:

		RenderDeviceVk* m_renderDevice;

		BufferData m_buffer;
		BUFFER_USAGE m_usage;

	};
}