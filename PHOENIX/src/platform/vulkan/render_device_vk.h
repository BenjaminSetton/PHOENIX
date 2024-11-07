#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/render_device.h"

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

	private:

		VkInstance m_vkInstance;
		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;

	};
}