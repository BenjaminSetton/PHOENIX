#pragma once

#include "types/basic_types.h"
#include "types/graphics_api.h"
#include "types/status_code.h"

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"

namespace PHX
{
	// ALLOCATIONS
	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo);

	STATUS_CODE InitializeGraphics(GRAPHICS_API api);
	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo);
	STATUS_CODE CreateSwapChain(const SwapChainCreateInfo& createInfo);

	// GETTERS
	[[nodiscard]] std::shared_ptr<IWindow> GetWindow();
	[[nodiscard]] std::shared_ptr<IRenderDevice> GetRenderDevice();
	[[nodiscard]] std::shared_ptr<ISwapChain> GetSwapChain();
}