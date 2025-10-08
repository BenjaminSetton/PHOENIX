
#include "object_factory.h"

#include "platform/vulkan/core_vk.h"
#include "platform/vulkan/render_device_vk.h"
#include "platform/vulkan/swap_chain_vk.h"

#include "core/global_settings.h"
#include "utils/logger.h"
#include "utils/sanity.h"

#if defined(PHX_WINDOWS)
#include "../platform/win64/window_win64.h"
#else
#error Invalid platform!
#endif

namespace PHX
{
	namespace OBJ_FACTORY
	{
		STATUS_CODE CreateCoreObjects(IWindow* pWindow)
		{
			auto& settings = GetSettings();
			switch (settings.backendAPI)
			{
			case GRAPHICS_API::VULKAN:
			{
				return CoreVk::Get().Initialize(pWindow);
			}
			}

			// Nothing was created
			return STATUS_CODE::ERR_INTERNAL;
		}

		IWindow* CreateWindow(const WindowCreateInfo& createInfo)
		{
#if defined(PHX_WINDOWS)
			return new WindowWin64(createInfo);
#else
#    error Invalid platform!
#endif
		}

		IRenderDevice* CreateRenderDevice(const RenderDeviceCreateInfo& createInfo)
		{
			auto& settings = GetSettings();
			switch (settings.backendAPI)
			{
			case GRAPHICS_API::VULKAN:
			{
				return new RenderDeviceVk(createInfo);
			}
			}

			return nullptr;
		}
	}
}