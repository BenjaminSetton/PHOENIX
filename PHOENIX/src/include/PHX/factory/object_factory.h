#pragma once

#include <memory>

#include "../types/basic_types.h"
#include "PHX/interface/render_device.h"
#include "PHX/interface/window.h"

namespace PHX
{
	enum class GRAPHICS_API : u8
	{
		VULKAN = 0,
		OPENGL,
		DIRECTX,
		INVALID
	};

	class ObjectFactory
	{
	public:

		static ObjectFactory& Get()
		{
			static ObjectFactory instance;
			return instance;
		}

		void SelectGraphicsAPI(GRAPHICS_API api);

		std::shared_ptr<IWindow> CreateWindow(const WindowCreateInfo& createInfo);
		std::shared_ptr<IRenderDevice> CreateRenderDevice(const RenderDeviceCreateInfo& createInfo);

	private:

		ObjectFactory();

	private:

		GRAPHICS_API m_selectedAPI;
	};
}