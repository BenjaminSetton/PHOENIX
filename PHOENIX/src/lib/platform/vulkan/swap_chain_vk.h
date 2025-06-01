#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"
#include "PHX/types/status_code.h"
#include "render_device_vk.h"
#include "texture_vk.h"

namespace PHX
{
	class SwapChainVk : public ISwapChain
	{
	public:

		explicit SwapChainVk(RenderDeviceVk* pRenderDevice, const SwapChainCreateInfo& createInfo);
		~SwapChainVk();

		ITexture* GetCurrentImage() const override;
		u32 GetImageCount() const override;
		u32 GetCurrentImageIndex() const override;
		STATUS_CODE Present() override;
		void Resize(u32 newWidth, u32 newHeight) override;

		STATUS_CODE AcquireNextImage(VkSemaphore imageAvailableSemaphore);

		VkSwapchainKHR GetSwapChain() const;
		VkFormat GetSwapChainFormat() const;
		u32 GetCurrentWidth() const;
		u32 GetCurrentHeight() const;
		u32 GetImageViewCount() const;

	private:

		STATUS_CODE CreateSwapChain(RenderDeviceVk* pRenderDevice, u32 width, u32 height, bool enableVSync);
		STATUS_CODE CreateSwapChainImageViews(RenderDeviceVk* pRenderDevice, VkFormat imageFormat);
		void DestroySwapChain();
		bool IsValid() const;
		void OnSwapChainOutdated();

	private:

		RenderDeviceVk* m_renderDevice; // Might want to look into a better solution

		VkSwapchainKHR m_swapChain;
		VkFormat m_format;
		u32 m_width;
		u32 m_height;
		std::vector<TextureVk*> m_images;
		u32 m_currImageIndex;
		u32 m_imageCount;
		bool m_isVSyncEnabled;
	};
}