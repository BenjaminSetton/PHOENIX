
#include "PHX/factory/object_factory.h"

#include "../platform/vulkan/render_device_vk.h"
#include "../utils/logger.h"
#include "../utils/sanity.h"

#if defined(PHX_WINDOWS)
#include "../platform/win64/window_win64.h"
#else
#    error Invalid platform!
#endif

namespace PHX
{
	void ObjectFactory::SelectGraphicsAPI(GRAPHICS_API api)
	{
		m_selectedAPI = api;
	}

	std::shared_ptr<IWindow> ObjectFactory::CreateWindow(const WindowCreateInfo& createInfo)
	{
#if defined(PHX_WINDOWS)
		return std::make_shared<WindowWin64>(createInfo);
#else
#    error Invalid platform!
#endif
	}

	std::shared_ptr<IRenderDevice> ObjectFactory::CreateRenderDevice(const RenderDeviceCreateInfo& createInfo)
	{
		if (m_selectedAPI != GRAPHICS_API::INVALID)
		{
			switch (m_selectedAPI)
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
			case GRAPHICS_API::DIRECTX:
			{
				TODO();
				break;
			}
			}
		}
		else
		{
			LogError("Failed to create render device. A graphics API has not been selected!");
		}

		return nullptr;
	}

	ObjectFactory::ObjectFactory() : m_selectedAPI(GRAPHICS_API::INVALID)
	{
	}
}