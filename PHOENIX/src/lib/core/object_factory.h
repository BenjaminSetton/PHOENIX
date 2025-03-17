#pragma once

#include <memory>

#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	namespace OBJ_FACTORY
	{
		STATUS_CODE CreateCoreObjects(IWindow* pWindow);

		IWindow* CreateWindow(const WindowCreateInfo& createInfo);
		IRenderDevice* CreateRenderDevice(const RenderDeviceCreateInfo& createInfo);
		ISwapChain* CreateSwapChain(const SwapChainCreateInfo& createInfo);
	}
}