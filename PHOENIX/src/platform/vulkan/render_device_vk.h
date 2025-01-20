#pragma once

#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "PHX/interface/render_device.h"
#include "PHX/types/queue_type.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	class RenderDeviceVk : public IRenderDevice
	{
	public:

		explicit RenderDeviceVk(const RenderDeviceCreateInfo& ci);
		~RenderDeviceVk() override;

		const char* GetDeviceName() const;

		STATUS_CODE AllocateBuffer() override;
		STATUS_CODE AllocateFramebuffer(const FramebufferCreateInfo& createInfo, IFramebuffer** out_framebuffer) override;
		STATUS_CODE AllocateCommandBuffer() override;
		STATUS_CODE AllocateTexture() override;
		STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader) override;

		void DeallocateFramebuffer(IFramebuffer* pFramebuffer) override;
		void DeallocateTexture(ITexture* pTexture) override;
		void DeallocateShader(IShader* pShader) override;

		VkDevice GetLogicalDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;

		VmaAllocator GetAllocator() const;

	private:

		STATUS_CODE CreateVMAAllocator();

		STATUS_CODE CreatePhysicalDevice(VkSurfaceKHR surface);
		STATUS_CODE CreateLogicalDevice(VkSurfaceKHR surface);

		STATUS_CODE AllocateDescriptorPool();

	private:

		VmaAllocator m_allocator;

		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;
		std::unordered_map<QUEUE_TYPE, VkQueue> m_queues;

		// Physical device cache
		VkPhysicalDeviceProperties m_physicalDeviceProperties;
		VkPhysicalDeviceFeatures m_physicalDeviceFeatures;
		VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

		// Descriptor pool
		VkDescriptorPool m_descriptorPool;
	};
}