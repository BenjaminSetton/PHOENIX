
#include "object_factory.h"

#include "../platform/vulkan/core/instance_vk.h"
#include "../platform/vulkan/core/surface_vk.h"
#include "../platform/vulkan/render_device_vk.h"
#include "../platform/vulkan/swap_chain_vk.h"
#include "../utils/logger.h"
#include "../utils/sanity.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/window.h"

#if defined(PHX_WINDOWS)
#include "../platform/win64/window_win64.h"
#else
#    error Invalid platform!
#endif

namespace PHX
{
	namespace OBJ_FACTORY
	{
		std::shared_ptr<IWindow> CreateWindow(const WindowCreateInfo& createInfo)
		{
	#if defined(PHX_WINDOWS)
			return std::make_shared<WindowWin64>(createInfo);
	#else
	#    error Invalid platform!
	#endif
		}

		static STATUS_CODE CreateCoreObjects(GRAPHICS_API api, IWindow* pWindow)
		{
			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				InstanceVk* instance = new InstanceVk(true);
				SurfaceVk* surface = new SurfaceVk(instance->GetInstance(), pWindow);
				TODO();
				break;
			}
			case GRAPHICS_API::OPENGL:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX11:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX12:
			{
				TODO();
				break;
			}
			}
		}

		std::shared_ptr<IRenderDevice> CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, GRAPHICS_API api)
		{
			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				return std::make_shared<RenderDeviceVk>(createInfo);
			}
			case GRAPHICS_API::OPENGL:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX11:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX12:
			{
				TODO();
				break;
			}
			}

			return nullptr;
		}

		std::shared_ptr<ISwapChain> CreateSwapChain(const SwapChainCreateInfo& createInfo, GRAPHICS_API api)
		{
			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				return std::make_shared<SwapChainVk>(createInfo);
			}
			case GRAPHICS_API::OPENGL:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX11:
			{
				TODO();
				break;
			}
			case GRAPHICS_API::DX12:
			{
				TODO();
				break;
			}
			}

			return nullptr;
		}
	}
}