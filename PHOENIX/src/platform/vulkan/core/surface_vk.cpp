
#include <vulkan/vulkan.h>

#include "surface_vk.h"

#if defined(PHX_WINDOWS)
#include "../../win64/window_win64.h"
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#else
#error Platform is not supported!
#endif

namespace PHX
{
	SurfaceVk::SurfaceVk(VkInstance vkInstance, IWindow* windowInterface) : m_surface(VK_NULL_HANDLE)
	{
#if defined(PHX_WINDOWS)
		WindowWin64* windowWin64 = dynamic_cast<WindowWin64*>(windowInterface);
		if (windowWin64 != nullptr)
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
			surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(windowWin64->GetHandle());

			vkCreateWin32SurfaceKHR(vkInstance, &surfaceCreateInfo, nullptr, &m_surface);
		}
#endif
	}

	SurfaceVk::~SurfaceVk()
	{
	}

	VkSurfaceKHR SurfaceVk::GetSurface() const
	{
		return m_surface;
	}
}

