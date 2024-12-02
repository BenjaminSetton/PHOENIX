#pragma once

#include <vulkan/vulkan.h>

namespace PHX
{
	// Forward declarations
	class IWindow;

	class SurfaceVk
	{
	public:

		explicit SurfaceVk(VkInstance vkInstance, IWindow* windowInterface);
		~SurfaceVk();

		VkSurfaceKHR GetSurface() const;

	private:

		VkSurfaceKHR m_surface;
	};
}