#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "PHX/interface/render_device.h"
#include "PHX/types/queue_type.h"

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

		VkInstance GetInstance() const;
		VkDevice GetLogicalDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;

	private:

		void CreateVkInstance(bool enableValidation);
		void CreatePhysicalDevice(VkSurfaceKHR surface);
		void CreateLogicalDevice(bool enableValidation, VkSurfaceKHR surface);

	private:

		VkInstance m_vkInstance;
		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;
		std::unordered_map<QUEUE_TYPE, VkQueue> m_queues;

	};
}