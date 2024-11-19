#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"

namespace PHX
{
	class SwapChainVk : public ISwapChain
	{
	public:

		SwapChainVk(const SwapChainCreateInfo& createInfo);
		~SwapChainVk();

		VkSurfaceKHR GetSurface() const;
		VkSwapchainKHR GetSwapChain() const;
		VkFormat GetSwapChainFormat() const;
		u32 GetCurrentWidth() const;
		u32 GetCurrentHeight() const;
		u32 GetImageViewCount() const;
		VkImageView GetImageViewAt(u32 index) const;

	private:

		void CreateSurface(VkInstance vkInstance, IWindow* windowInterface);
		void CreateSwapChain(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, u32 width, u32 height, bool enableVSync);
		void CreateSwapChainImageViews(VkDevice logicalDevice, u32 imageCount, VkFormat imageFormat);

	private:

		VkSurfaceKHR m_surface;
		VkSwapchainKHR m_swapChain;
		VkFormat m_format;
		u32 m_width;
		u32 m_height;
		std::vector<VkImageView> m_imageViews; // TODO - Replace with ITexture later on

	};
}