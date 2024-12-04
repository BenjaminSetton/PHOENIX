
#include "object_factory.h"

#include "../platform/vulkan/core_vk.h"
#include "../platform/vulkan/render_device_vk.h"
#include "../platform/vulkan/swap_chain_vk.h"

#include "../utils/logger.h"
#include "../utils/sanity.h"

#if defined(PHX_WINDOWS)
#include "../platform/win64/window_win64.h"
#else
#error Invalid platform!
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

		STATUS_CODE CreateCoreObjects(GRAPHICS_API api, std::shared_ptr<IWindow> pWindow)
		{
			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				// Only enable validation in debug builds. Later on we'll want to
				// expose this as an option to the user
#if defined(PHX_DEBUG)
				bool enableValidation = true;
#else
				bool enableValidation = false;
#endif
				return CoreVk::Get().Initialize(enableValidation, pWindow);
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

			// Nothing was created
			return STATUS_CODE::ERR;
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