#pragma once

#include <vector>
#include <vulkan/vulkan.h>

///
/// Contains swap chain helper functions/types that must be called from more than one place.
/// For example, the render device and the swap chain implementations must call QuerySwapChainSupport()
/// 
namespace PHX
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
}