#pragma once

#include <vulkan/vulkan.h>

namespace PHX
{
	class InstanceVk
	{
	public:

		explicit InstanceVk(bool enableValidationLayers);
		~InstanceVk();

		VkInstance GetInstance() const;

	private:

		VkInstance m_instance;
	};
}