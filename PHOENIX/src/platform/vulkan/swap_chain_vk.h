#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	class SwapChainVk : public ISwapChain
	{
	public:

		explicit SwapChainVk(const SwapChainCreateInfo& createInfo);
		~SwapChainVk();

		VkSwapchainKHR GetSwapChain() const;
		VkFormat GetSwapChainFormat() const;
		u32 GetCurrentWidth() const;
		u32 GetCurrentHeight() const;
		u32 GetImageViewCount() const;
		VkImageView GetImageViewAt(u32 index) const;

	private:

		STATUS_CODE CreateSwapChain(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, u32 width, u32 height, bool enableVSync);
		STATUS_CODE CreateSwapChainImageViews(VkDevice logicalDevice, u32 imageCount, VkFormat imageFormat);

	private:

		VkSwapchainKHR m_swapChain;
		VkFormat m_format;
		u32 m_width;
		u32 m_height;
		std::vector<VkImageView> m_imageViews; // TODO - Replace with ITexture later on

	};
}