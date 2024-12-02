#pragma once

#include <memory>

#include "PHX/types/basic_types.h"
#include "PHX/types/graphics_api.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Forward declarations
	class IWindow;
	class IRenderDevice;
	class ISwapChain;

	namespace OBJ_FACTORY
	{
		static std::shared_ptr<IWindow> CreateWindow(const WindowCreateInfo& createInfo);

		static STATUS_CODE CreateCoreObjects(GRAPHICS_API api, IWindow* pWindow);
		static std::shared_ptr<IRenderDevice> CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, GRAPHICS_API api);
		static std::shared_ptr<ISwapChain> CreateSwapChain(const SwapChainCreateInfo& createInfo, GRAPHICS_API api);
	}
}