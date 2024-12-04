#pragma once

#include <memory>

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/graphics_api.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	namespace OBJ_FACTORY
	{
		std::shared_ptr<IWindow> CreateWindow(const WindowCreateInfo& createInfo);

		STATUS_CODE CreateCoreObjects(GRAPHICS_API api, std::shared_ptr<IWindow> pWindow);
		std::shared_ptr<IRenderDevice> CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, GRAPHICS_API api);
		std::shared_ptr<ISwapChain> CreateSwapChain(const SwapChainCreateInfo& createInfo, GRAPHICS_API api);
	}
}