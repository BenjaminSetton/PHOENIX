
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "swap_chain_vk.h"

#include "../../utils/logger.h"
#include "../../utils/math.h"
#include "../../utils/sanity.h"
#include "core_vk.h"
#include "PHX/types/queue_type.h"
#include "utils/queue_family_indices.h"
#include "utils/swap_chain_helpers.h"
#include "utils/texture_type_converter.h"

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
		RenderDeviceVk* renderDevice = static_cast<RenderDeviceVk*>(createInfo.renderDevice);
		if (createInfo.renderDevice == nullptr)
		{
			LogError("Render device pointer is null. Failed to create swap chain!");
			return;
		}

		STATUS_CODE res = CreateSwapChain(renderDevice, createInfo.width, createInfo.height, createInfo.enableVSync);
		if (res == STATUS_CODE::SUCCESS)
		{
			m_renderDevice = renderDevice;
		}
	}

	SwapChainVk::~SwapChainVk()
	{
		DestroySwapChain();
	}

	ITexture* SwapChainVk::GetImage(u32 imageIndex) const
	{
		if (imageIndex < m_images.size())
		{
			return m_images[imageIndex];
		}

		return nullptr;
	}

	u32 SwapChainVk::GetImageCount() const
	{
		return static_cast<u32>(m_images.size());
	}

	u32 SwapChainVk::GetCurrentImageIndex() const
	{
		return m_currImageIndex;
	}

	STATUS_CODE SwapChainVk::Present() const
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 0; // TODO
		presentInfo.pWaitSemaphores = nullptr;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapChain;
		presentInfo.pImageIndices = &m_currImageIndex;
		presentInfo.pResults = nullptr;

		VkQueue presentQueue = m_renderDevice->GetQueue(QUEUE_TYPE::PRESENT);
		VkResult res = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
		{
			// Add callback for swapchain resize instead, and let the client select the new dimensions
			TODO();
			//CreateSwapChain();
		}
		else if (res != VK_SUCCESS)
		{
			LogError("Failed to present swap chain image! Got error: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE SwapChainVk::AcquireNextImage()
	{
		VkResult resVk = vkAcquireNextImageKHR(m_renderDevice->GetLogicalDevice(), m_swapChain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &m_currImageIndex);
		if (resVk == VK_ERROR_OUT_OF_DATE_KHR || resVk == VK_SUBOPTIMAL_KHR)
		{
			// Use a callback for swapchain being out of date
			TODO();
		}
		else if (resVk != VK_SUCCESS)
		{
			LogError("Failed to acquire swap chain image! Got error: %s", string_VkResult(resVk));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
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
		return static_cast<u32>(m_images.size());
	}

	STATUS_CODE SwapChainVk::CreateSwapChain(RenderDeviceVk* pRenderDevice, u32 width, u32 height, bool enableVSync)
	{
		// Clean up old swap chain data if necessary. This can happen when swap chain has already been created but
		// needs to be resized because the window dimensions changed
		if (IsValid())
		{
			DestroySwapChain();
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();
		VkPhysicalDevice physicalDevice = pRenderDevice->GetPhysicalDevice();

		const VkSurfaceKHR surface = CoreVk::Get().GetSurface();

		SwapChainSupportDetails details = QuerySwapChainSupport(physicalDevice, surface);
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
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
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

		VkResult res = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &m_swapChain);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create swap chain! Got error: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		// Get the number of images, then we use the count to create the image views below
		res = vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, nullptr);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to get swapchain images! Got error: %s", string_VkResult(res));
			DestroySwapChain();
			return STATUS_CODE::ERR;
		}

		m_format = surfaceFormat.format;
		m_width = extent.width;
		m_height = extent.height;

		if (CreateSwapChainImageViews(pRenderDevice, imageCount, surfaceFormat.format) != STATUS_CODE::SUCCESS)
		{
			DestroySwapChain();
			return STATUS_CODE::ERR;
		}

		m_currImageIndex = 0;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE SwapChainVk::CreateSwapChainImageViews(RenderDeviceVk* pRenderDevice, u32 imageCount, VkFormat imageFormat)
	{
		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		std::vector<VkImage> swapChainImages(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, swapChainImages.data());

		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = imageFormat;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		std::vector<VkImageView> imageViews;
		imageViews.resize(imageCount);

		for (uint32_t i = 0; i < imageCount; i++)
		{
			createInfo.image = swapChainImages[i];
			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &(imageViews.at(i))) != VK_SUCCESS)
			{
				LogError("Failed to create one or more swap chain image views!");
				return STATUS_CODE::ERR;
			}
		}

		// Once all image views are successfully created, create 
		// internal texture objects from swap chain image views
		TextureBaseCreateInfo texBaseCI{};
		texBaseCI.width = m_width;
		texBaseCI.height = m_height;
		texBaseCI.mipLevels = 1;
		texBaseCI.generateMips = false;
		texBaseCI.usageFlags = static_cast<UsageTypeFlags>(USAGE_TYPE::COLOR_ATTACHMENT);
		texBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;
		texBaseCI.format = TEX_UTILS::ConvertSurfaceFormat(m_format);

		m_images.reserve(imageCount);
		for (u32 i = 0; i < imageCount; i++)
		{
			m_images.push_back(new TextureVk(pRenderDevice, texBaseCI, imageViews.at(i)));
		}

		return STATUS_CODE::SUCCESS;
	}
	
	void SwapChainVk::DestroySwapChain()
	{
		if (m_renderDevice == nullptr)
		{
			LogError("Failed to destroy swap chain.! Render device is null");
			return;
		}

		if (m_swapChain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(m_renderDevice->GetLogicalDevice(), m_swapChain, nullptr);
		}

		for (auto& image : m_images)
		{
			m_renderDevice->DeallocateTexture(image);
		}
	}

	bool SwapChainVk::IsValid() const
	{
		return (m_swapChain != VK_NULL_HANDLE && m_images.size() > 0);
	}
}