#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/swap_chain.h"

namespace PHX
{
	class SwapChainVk : public ISwapChain
	{
	public:

		SwapChainVk() = default;
		~SwapChainVk() = default;

		bool Create(const SwapChainCreateInfo& createInfo) override;

	private:

		VkSurfaceKHR m_surface;

	};
}