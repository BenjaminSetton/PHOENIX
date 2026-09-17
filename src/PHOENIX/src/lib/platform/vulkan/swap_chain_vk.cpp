
#include <algorithm>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "swap_chain_vk.h"

#include "BSL/logger.h"
#include "BSL/math.h"
#include "BSL/sanity.h"
#include "core/global_settings.h"
#include "core/profiling.h"
#include "core_vk.h"
#include "PHX/types/queue_type.h"
#include "utils/swap_chain_utils.h"
#include "utils/texture_type_converter.h"
#include "utils/debug_utils.h"

using namespace BSL;

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

	static bool IsPresentModeSupported(const std::vector<VkPresentModeKHR>& availablePresentModes, VkPresentModeKHR presentMode)
	{
		return std::find(availablePresentModes.begin(), availablePresentModes.end(), presentMode) != availablePresentModes.end();
	}

	static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes, PRESENT_MODE requested)
	{
		VkPresentModeKHR requestedVkMode = ConvertPresentMode(requested);

		std::vector<VkPresentModeKHR> candidates;
		switch (requested)
		{
		case PRESENT_MODE::IMMEDIATE:
			candidates = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR };
			break;
		case PRESENT_MODE::MAILBOX:
			candidates = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
			break;
		case PRESENT_MODE::FIFO_RELAXED:
			candidates = { VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_FIFO_KHR };
			break;
		case PRESENT_MODE::FIFO:
			candidates = { VK_PRESENT_MODE_FIFO_KHR };
			break;
		}

		for (VkPresentModeKHR candidate : candidates)
		{
			if (IsPresentModeSupported(availablePresentModes, candidate))
			{
				// Pick the first supported mode, even if it's not the requested one
				if (candidate == requestedVkMode)
				{
					LogInfo("Selected swap chain present mode \"%s\"", string_VkPresentModeKHR(candidate));
				}
				else
				{
					LogWarning("Requested swap chain present mode \"%s\" is not supported, falling back to \"%s\"",
						string_VkPresentModeKHR(requestedVkMode), string_VkPresentModeKHR(candidate));
				}
				return candidate;
			}
		}

		// FIFO is guaranteed by the spec; if even that's missing just return it anyway and let validation complain
		ASSERT_ALWAYS("FIFO present mode is not supported by this device! This should never happen");
		return VK_PRESENT_MODE_FIFO_KHR;
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
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to create swap chain. Render device pointer is null!");
			return;
		}

		STATUS_CODE res = CreateSwapChain(pRenderDevice, createInfo.width, createInfo.height, createInfo.presentMode);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_renderDevice = pRenderDevice;
		m_presentMode = createInfo.presentMode;
	}

	SwapChainVk::~SwapChainVk()
	{
		DestroySwapChain();
	}

	TextureHandle SwapChainVk::GetCurrentImage() const
	{
		PROFILE_SCOPE("SwapChainVk_GetCurrentImage");

		return m_images[m_currImageIndex];
	}

	u32 SwapChainVk::GetImageCount() const
	{
		return m_imageCount;
	}

	u32 SwapChainVk::GetCurrentImageIndex() const
	{
		PROFILE_SCOPE("SwapChainVk_GetCurrentImageIndex");

		return m_currImageIndex;
	}

	STATUS_CODE SwapChainVk::Present()
	{
		PROFILE_SCOPE("SwapChainVk_Present");

		VkSemaphore renderFinishedSemaphore = GetRenderFinishedSemaphore();

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
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

		STATUS_CODE res = CreateSwapChain(m_renderDevice, newWidth, newHeight, m_presentMode);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to resize swap chain!");
			return;
		}
	}

	u32 SwapChainVk::GetWidth() const
	{
		return m_width;
	}

	u32 SwapChainVk::GetHeight() const
	{
		return m_height;
	}

	STATUS_CODE SwapChainVk::AcquireNextImage(VkSemaphore imageAvailableSemaphore)
	{
		PROFILE_SCOPE("SwapChainVk_AcquireNextImage");

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

	VkSemaphore SwapChainVk::GetRenderFinishedSemaphore() const
	{
		return m_renderFinishedSemaphores[m_currImageIndex];
	}

	VkSwapchainKHR SwapChainVk::GetSwapChain() const
	{
		return m_swapChain;
	}

	VkFormat SwapChainVk::GetSwapChainFormat() const
	{
		return m_format;
	}

	u32 SwapChainVk::GetImageViewCount() const
	{
		return static_cast<u32>(m_images.size());
	}

	STATUS_CODE SwapChainVk::CreateSwapChain(RenderDeviceVk* pRenderDevice, u32 width, u32 height, PRESENT_MODE presentMode)
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
		VkPresentModeKHR vkPresentMode = ChooseSwapPresentMode(details.presentModes, presentMode);
		VkExtent2D extent = ChooseSwapChainExtent(details.capabilities, width, height);

		// Warn when we cannot use specified dimensions
		if (width != extent.width || height != extent.height)
		{
			LogWarning("Could not create swap chain with specified dimensions (%ux%u)! Using %ux%u instead",
				width, height, extent.width, extent.height);
		}

		// Mailbox needs an extra image, on top of the +1 above the minimum, so the compositor (e.g. DWM in windowed mode) 
		// holding one doesn't stall the acquireImage call
		const bool requiresExtraImage = (vkPresentMode == VK_PRESENT_MODE_MAILBOX_KHR);
		u32 imageCount = details.capabilities.minImageCount + (requiresExtraImage ? 2 : 1);
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
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Allow reads/writes from and to backbuffer

		uint32_t queueFamilyIndices[2] = { pRenderDevice->GetQueueFamilyIndex(QUEUE_TYPE::GRAPHICS), pRenderDevice->GetQueueFamilyIndex(QUEUE_TYPE::PRESENT) };

		if (queueFamilyIndices[0] != queueFamilyIndices[1])
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
			
			LogInfo("Using concurrent swap chain sharing mode with queue family indices %u and %u", queueFamilyIndices[0], queueFamilyIndices[1]);
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional

			LogInfo("Using exclusive swap chain sharing mode");
		}

		createInfo.preTransform = details.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = vkPresentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		// Swap chain
		VkResult resVk = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &m_swapChain);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to create swap chain! Got error: \"%s\"", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, reinterpret_cast<uint64_t>(m_swapChain), "SwapChain");

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

		// Render finished semaphores
		m_renderFinishedSemaphores.resize(m_imageCount);
		for (u32 i = 0; i < m_imageCount; i++)
		{
			VkSemaphoreCreateInfo semaphoreCI{};
			semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			resVk = vkCreateSemaphore(logicalDevice, &semaphoreCI, nullptr, &m_renderFinishedSemaphores[i]);
			if (resVk != VK_SUCCESS)
			{
				LogError("Failed to create render finished semaphore! Got error: \"%s\"", string_VkResult(resVk));
				DestroySwapChain();
				return STATUS_CODE::ERR_INTERNAL;
			}

			char semaphoreName[64];
			snprintf(semaphoreName, sizeof(semaphoreName), "RenderFinishedSemaphore_%u", i);
			DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_SEMAPHORE, reinterpret_cast<uint64_t>(m_renderFinishedSemaphores[i]), semaphoreName);
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

		for (uint32_t i = 0; i < m_imageCount; i++)
		{
			char imgName[64];
			snprintf(imgName, sizeof(imgName), "BackbufferImage_%u", i);
			DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_IMAGE, reinterpret_cast<uint64_t>(swapChainImages[i]), imgName);
		}

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

			char name[64];
			snprintf(name, sizeof(name), "BackbufferImageView_%u", i);
			DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_IMAGE_VIEW, reinterpret_cast<uint64_t>(imageViews.at(i)), name);
		}

		// Once all image views are successfully created, create 
		// internal texture objects from swap chain image views
		TextureBaseCreateInfo texBaseCI{};
		texBaseCI.pName = "Backbuffer";
		texBaseCI.width = m_width;
		texBaseCI.height = m_height;
		texBaseCI.mipLevels = 1;
		texBaseCI.generateMips = false;
		texBaseCI.usageFlags = USAGE_TYPE_FLAG_COLOR_ATTACHMENT;
		texBaseCI.sampleFlags = SAMPLE_COUNT::COUNT_1;
		texBaseCI.format = TEX_UTILS::ConvertSurfaceFormat(m_format);

		m_images.reserve(m_imageCount);
		for (u32 i = 0; i < m_imageCount; i++)
		{
			TextureHandle texture;
			STATUS_CODE res = pRenderDevice->AllocateSwapchainTexture(texBaseCI, imageViews.at(i), texture);
			if (res != STATUS_CODE::SUCCESS)
			{
				ASSERT_ALWAYS("Failed to allocate swapchain texture!");
				continue;
			}

			m_images.push_back(texture);
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

		for (VkSemaphore semaphore : m_renderFinishedSemaphores)
		{
			vkDestroySemaphore(m_renderDevice->GetLogicalDevice(), semaphore, nullptr);
		}
		m_renderFinishedSemaphores.clear();

		if (m_swapChain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(m_renderDevice->GetLogicalDevice(), m_swapChain, nullptr);
		}

		// TextureHandle is ref-counted, no need to manually clean up
		m_images.clear();
	}

	bool SwapChainVk::IsValid() const
	{
		return (m_swapChain != VK_NULL_HANDLE && m_images.size() > 0);
	}

	void SwapChainVk::OnSwapChainOutdated()
	{
		PROFILE_SCOPE("SwapChainVk_OnSwapChainOutdated");

		auto& settings = GetSettings();
		if (settings.swapChainOutdatedCallback == nullptr)
		{
			LogWarning("Swap chain is outdated but no callback was provided for swapChainOutdatedCallback in Settings!");
			return;
		}

		settings.swapChainOutdatedCallback();
	}
}