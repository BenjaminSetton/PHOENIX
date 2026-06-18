
#include "object_factory.h"

#include "platform/vulkan/core_vk.h"
#include "platform/vulkan/render_device_vk.h"
#include "platform/vulkan/swap_chain_vk.h"

#include "core/global_settings.h"
#include "core/handle/handle_utils.h"
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
			default:
			{
				ASSERT_ALWAYS("Failed to create core objects. Unsupported graphics API!");
				break;
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

		STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice)
		{
			// TODO - Move to appropriate storage
			static IRenderDevice* s_pRenderDevice = nullptr;

			auto& settings = GetSettings();
			switch (settings.backendAPI)
			{
			case GRAPHICS_API::VULKAN:
			{
				if (s_pRenderDevice != nullptr)
				{
					LogError("Cannot re-create render device. An instance already exists!");
				}
				else
				{
					s_pRenderDevice = new RenderDeviceVk(createInfo);
					if (s_pRenderDevice == nullptr)
					{
						LogError("Failed to create render device. Memory allocation failed!");
						return STATUS_CODE::ERR_INTERNAL;
					}
				}
				return HANDLE_UTILS::AllocateHandle(s_pRenderDevice, renderDevice);
			}
			default:
			{
				ASSERT_ALWAYS("Failed to create render device. Unsupported graphics API!");
				break;
			}
			}

			return STATUS_CODE::ERR_API;
		}
	}
}