
#include "swap_chain_utils.h"

#include "BSL/integral_types.h"
#include "BSL/sanity.h"

namespace PHX
{
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		u32 formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		u32 presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkPresentModeKHR ConvertPresentMode(PRESENT_MODE presentMode)
	{
		switch (presentMode)
		{
		case PRESENT_MODE::FIFO:         return VK_PRESENT_MODE_FIFO_KHR;
		case PRESENT_MODE::FIFO_RELAXED: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		case PRESENT_MODE::MAILBOX:      return VK_PRESENT_MODE_MAILBOX_KHR;
		case PRESENT_MODE::IMMEDIATE:    return VK_PRESENT_MODE_IMMEDIATE_KHR;
		}

		ASSERT_ALWAYS("Could not convert present mode to VkPresentModeKHR. Unknown present mode!");
		return VK_PRESENT_MODE_FIFO_KHR;
	}
}