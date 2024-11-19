
#include <vector>

#include "swap_chain_vk.h"

#include "../../utils/logger.h"
#include "../../utils/math.h"
#include "../../utils/sanity.h"
#include "PHX/types/queue_type.h"
#include "render_device_vk.h"
#include "utils/queue_family_indices.h"
#include "utils/swap_chain_helpers.h"

#if defined(PHX_WINDOWS)
#include "../win64/window_win64.h"
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#else
#error Platform is not supported!
#endif

namespace PHX
{
	static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats)
		{
			// TODO - Need a better way to choose swap chain surface format
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		// Return the first available format in this case I guess?
		if (availableFormats.size() > 0)
		{
			return availableFormats[0];
		}

		ASSERT_ALWAYS("Failed to find an appropriate swap chain surface format!");
		return {};
	}

	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, bool enableVSync)
	{
		bool mailboxValid = false;
		bool fifoValid = false;
		for (const auto& presentMode : availablePresentModes)
		{
			if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				mailboxValid = true;
			}
			else if (presentMode == VK_PRESENT_MODE_FIFO_KHR)
			{
				fifoValid = true;
			}
		}

		if (enableVSync)
		{
			if (fifoValid)
			{
				LogInfo("Selected FIFO present mode");
				return VK_PRESENT_MODE_FIFO_KHR;
			}
		}
		else
		{
			if (mailboxValid)
			{
				LogInfo("Selected mailbox present mode");
				return VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}

		// Default to immediate presentation (is this guaranteed to be available?)
		LogInfo("Defaulted to immediate mode presentation");
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	}

	static VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height)
	{
		if (capabilities.currentExtent.width != U32_MAX)
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent{width, height};

			actualExtent.width = Clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = Clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	SwapChainVk::SwapChainVk(const SwapChainCreateInfo& createInfo)
	{
		if (createInfo.renderDevice != nullptr)
		{
			LogError("Render device pointer is null. Failed to create swap chain!");
			return;
		}

		if (createInfo.window != nullptr)
		{
			LogError("Window pointer is null. Failed to create swap chain!");
			return;
		}

		RenderDeviceVk* renderDevice = dynamic_cast<RenderDeviceVk*>(createInfo.renderDevice);
		VkInstance vkInstance = renderDevice->GetInstance();
		VkDevice logicalDevice = renderDevice->GetLogicalDevice();
		VkPhysicalDevice physicalDevice = renderDevice->GetPhysicalDevice();

		CreateSurface(vkInstance, createInfo.window);
		CreateSwapChain(logicalDevice, physicalDevice, createInfo.width, createInfo.height, createInfo.enableVSync);
		
	}

	SwapChainVk::~SwapChainVk()
	{
		TODO();
	}

	VkSurfaceKHR SwapChainVk::GetSurface() const
	{
		return m_surface;
	}

	VkSwapchainKHR SwapChainVk::GetSwapChain() const
	{
		return m_swapChain;
	}

	VkFormat SwapChainVk::GetSwapChainFormat() const
	{
		return m_format;
	}

	u32 SwapChainVk::GetCurrentWidth() const
	{
		return m_width;
	}

	u32 SwapChainVk::GetCurrentHeight() const
	{
		return m_height;
	}

	u32 SwapChainVk::GetImageViewCount() const
	{
		return static_cast<u32>(m_imageViews.size());
	}

	VkImageView SwapChainVk::GetImageViewAt(u32 index) const
	{
		if (index < m_imageViews.size())
		{
			return m_imageViews.at(index);
		}
		return VK_NULL_HANDLE;
	}

	void SwapChainVk::CreateSurface(VkInstance vkInstance, IWindow* windowInterface)
	{
#if defined(PHX_WINDOWS)
		WindowWin64* windowWin64 = dynamic_cast<WindowWin64*>(windowInterface);
		if (windowWin64 != nullptr)
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
			surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(windowWin64->GetHandle());

			vkCreateWin32SurfaceKHR(vkInstance, &surfaceCreateInfo, nullptr, &m_surface);
		}
#endif
	}

	void SwapChainVk::CreateSwapChain(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, u32 width, u32 height, bool enableVSync)
	{
		SwapChainSupportDetails details = QuerySwapChainSupport(physicalDevice, m_surface);
		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(details.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(details.presentModes, enableVSync);
		VkExtent2D extent = ChooseSwapChainExtent(details.capabilities, width, height);

		uint32_t imageCount = details.capabilities.minImageCount + 1;
		if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
		{
			imageCount = details.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, m_surface);
		uint32_t queueFamilyIndices[2] = { indices.GetIndex(QUEUE_TYPE::GRAPHICS), indices.GetIndex(QUEUE_TYPE::PRESENT) };

		if (queueFamilyIndices[0] != queueFamilyIndices[1])
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = details.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
		{
			ASSERT_ALWAYS("Failed to create swap chain!");
		}

		// Get the number of images, then we use the count to create the image views below
		vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, nullptr);

		m_format = surfaceFormat.format;
		m_width = extent.width;
		m_height = extent.height;

		CreateSwapChainImageViews(logicalDevice, imageCount, surfaceFormat.format);
	}

	void SwapChainVk::CreateSwapChainImageViews(VkDevice logicalDevice, u32 imageCount, VkFormat imageFormat)
	{
		std::vector<VkImage> swapChainImages(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, swapChainImages.data());

		m_imageViews.reserve(imageCount);

		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = imageFormat;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		for (uint32_t i = 0; i < imageCount; i++)
		{
			createInfo.image = swapChainImages[i];
			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS)
			{
				ASSERT_ALWAYS("Failed to create one or more swap chain image views!");
			}
		}
	}

}