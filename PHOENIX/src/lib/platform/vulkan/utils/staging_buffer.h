#pragma once

#include "PHX/interface/buffer.h"

#include "../render_device_vk.h"
#include "buffer_utils.h"

namespace PHX
{
	class StagingBufferVk
	{
	public:

		explicit StagingBufferVk(RenderDeviceVk* pRenderDevice, const BufferCreateInfo& createInfo);
		~StagingBufferVk();

		StagingBufferVk(const StagingBufferVk& other) = delete;
		StagingBufferVk& operator=(const StagingBufferVk& other) = delete;

		STATUS_CODE CopyData(const void* data, u64 sizeBytes);

		VkBuffer GetBuffer() const;
		bool IsValid() const;

	private:

		RenderDeviceVk* m_renderDevice;

		BufferData m_buffer;
	};
}