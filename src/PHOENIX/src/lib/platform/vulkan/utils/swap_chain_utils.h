#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"

namespace PHX
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkPresentModeKHR ConvertPresentMode(PRESENT_MODE presentMode);
}