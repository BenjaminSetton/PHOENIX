#pragma once

#include <unordered_map>
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
		~RenderDeviceVk();

		bool AllocateBuffer() override;
		bool AllocateCommandBuffer() override;
		bool AllocateTexture() override;
		bool AllocateShader() override;

		VkDevice GetLogicalDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;

	private:

		STATUS_CODE CreatePhysicalDevice(VkSurfaceKHR surface);
		STATUS_CODE CreateLogicalDevice(VkSurfaceKHR surface);

	private:

		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;
		std::unordered_map<QUEUE_TYPE, VkQueue> m_queues;

		// Physical device cache
		VkPhysicalDeviceProperties physicalDeviceProperties;
		VkPhysicalDeviceFeatures physicalDeviceFeatures;
		VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	};
}