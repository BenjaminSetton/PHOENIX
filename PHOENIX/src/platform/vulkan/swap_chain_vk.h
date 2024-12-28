#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"
#include "PHX/types/status_code.h"
#include "texture_vk.h"

namespace PHX
{
	class SwapChainVk : public ISwapChain
	{
	public:

		explicit SwapChainVk(const SwapChainCreateInfo& createInfo);
		~SwapChainVk();

		ITexture* GetImage(u32 imageIndex) const override;

		VkSwapchainKHR GetSwapChain() const;
		VkFormat GetSwapChainFormat() const;
		u32 GetCurrentWidth() const;
		u32 GetCurrentHeight() const;
		u32 GetImageViewCount() const;

	private:

		STATUS_CODE CreateSwapChain(RenderDeviceVk* pRenderDevice, u32 width, u32 height, bool enableVSync);
		STATUS_CODE CreateSwapChainImageViews(RenderDeviceVk* pRenderDevice, u32 imageCount, VkFormat imageFormat);

	private:

		VkSwapchainKHR m_swapChain;
		VkFormat m_format;
		u32 m_width;
		u32 m_height;
		std::vector<TextureVk*> m_images; // TODO - Replace with ITexture later on

	};
}