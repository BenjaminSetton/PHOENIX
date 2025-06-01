
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "swap_chain_vk.h"

#include "core/global_settings.h"
#include "core_vk.h"
#include "PHX/types/queue_type.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/queue_family_indices.h"
#include "utils/sanity.h"
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
				LogInfo("Selected swap chain FIFO present mode");
				return VK_PRESENT_MODE_FIFO_KHR;
			}
		}
		else
		{
			if (mailboxValid)
			{
				LogInfo("Selected swap chain mailbox present mode");
				return VK_PRESENT_MODE_MAILBOX_KHR;
			}
		}

		// Default to immediate presentation (is this guaranteed to be available?)
		LogInfo("Defaulted to swap chain immediate mode presentation");
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

	SwapChainVk::SwapChainVk(RenderDeviceVk* pRenderDevice, const SwapChainCreateInfo& createInfo)
	{
		RenderDeviceVk* renderDevice = static_cast<RenderDeviceVk*>(pRenderDevice);
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to create swap chain. Render device pointer is null!");
			return;
		}

		STATUS_CODE res = CreateSwapChain(renderDevice, createInfo.width, createInfo.height, createInfo.enableVSync);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_renderDevice = renderDevice;
		m_isVSyncEnabled = createInfo.enableVSync;
	}

	SwapChainVk::~SwapChainVk()
	{
		DestroySwapChain();
	}

	ITexture* SwapChainVk::GetCurrentImage() const
	{
		return m_images[m_currImageIndex];
	}

	u32 SwapChainVk::GetImageCount() const
	{
		return m_imageCount;
	}

	u32 SwapChainVk::GetCurrentImageIndex() const
	{
		return m_currImageIndex;
	}

	STATUS_CODE SwapChainVk::Present()
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
			OnSwapChainOutdated();
		}
		else if (res != VK_SUCCESS)
		{
			LogError("Failed to present swap chain image! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	void SwapChainVk::Resize(u32 newWidth, u32 newHeight)
	{
		if (newWidth == 0 && newHeight == 0)
		{
			// Don't do anything when extent is (0, 0); window was probably minimized
			return;
		}

		LogInfo("Resized swap chain from %ux%u to %ux%u", m_width, m_height, newWidth, newHeight);

		// Invalidate the old swapchain framebuffers which are still stored in the render device's framebuffer cache
		m_renderDevice->InvalidateBackbufferFramebuffers();

		STATUS_CODE res = CreateSwapChain(m_renderDevice, newWidth, newHeight, m_isVSyncEnabled);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to resize swap chain!");
			return;
		}
	}

	STATUS_CODE SwapChainVk::AcquireNextImage(VkSemaphore imageAvailableSemaphore)
	{
		VkResult resVk = vkAcquireNextImageKHR(m_renderDevice->GetLogicalDevice(), m_swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &m_currImageIndex);
		if (resVk == VK_ERROR_OUT_OF_DATE_KHR || resVk == VK_SUBOPTIMAL_KHR)
		{
			OnSwapChainOutdated();
		}
		else if (resVk != VK_SUCCESS)
		{
			LogError("Failed to acquire swap chain image! Got error: \"%s\"", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
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

		// Swap chain
		VkResult resVk = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &m_swapChain);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to create swap chain! Got error: \"%s\"", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Get the number of images, then we use the count to create the image views below
		resVk = vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &imageCount, nullptr);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to get swapchain images! Got error: \"%s\"", string_VkResult(resVk));
			DestroySwapChain();
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_format = surfaceFormat.format;
		m_width = extent.width;
		m_height = extent.height;
		m_imageCount = imageCount;

		// Image views
		STATUS_CODE res = CreateSwapChainImageViews(pRenderDevice, surfaceFormat.format);
		if (res != STATUS_CODE::SUCCESS)
		{
			DestroySwapChain();
			return res;
		}

		m_currImageIndex = 0;

		LogInfo("Successfully created swap chain with dimensions %ux%u!", m_width, m_height);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE SwapChainVk::CreateSwapChainImageViews(RenderDeviceVk* pRenderDevice, VkFormat imageFormat)
	{
		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		std::vector<VkImage> swapChainImages(m_imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, m_swapChain, &m_imageCount, swapChainImages.data());

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
		imageViews.resize(m_imageCount);

		for (uint32_t i = 0; i < m_imageCount; i++)
		{
			createInfo.image = swapChainImages[i];
			if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &(imageViews.at(i))) != VK_SUCCESS)
			{
				LogError("Failed to create one or more swap chain image views!");
				return STATUS_CODE::ERR_INTERNAL;
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

		m_images.reserve(m_imageCount);
		for (u32 i = 0; i < m_imageCount; i++)
		{
			m_images.push_back(new TextureVk(pRenderDevice, texBaseCI, imageViews.at(i)));
		}

		return STATUS_CODE::SUCCESS;
	}
	
	void SwapChainVk::DestroySwapChain()
	{
		if (m_renderDevice == nullptr)
		{
			LogError("Failed to destroy swap chain! Render device is null");
			return;
		}

		if (m_swapChain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(m_renderDevice->GetLogicalDevice(), m_swapChain, nullptr);
		}

		for (u32 i = 0; i < m_images.size(); i++)
		{
			ITexture* image = m_images[i];
			m_renderDevice->DeallocateTexture(&image);
		}
		m_images.clear();
	}

	bool SwapChainVk::IsValid() const
	{
		return (m_swapChain != VK_NULL_HANDLE && m_images.size() > 0);
	}

	void SwapChainVk::OnSwapChainOutdated()
	{
		auto& settings = GetSettings();
		if (settings.swapChainOutdatedCallback == nullptr)
		{
			LogWarning("Swap chain is outdated but no callback was provided for swapChainOutdatedCallback in Settings!");
			return;
		}

		settings.swapChainOutdatedCallback();
	}
}