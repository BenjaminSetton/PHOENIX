#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "PHX/types/status_code.h"
#include "PHX/interface/window.h"

namespace PHX
{
	class CoreVk
	{
	public:

		static CoreVk& Get()
		{
			static CoreVk instance;
			return instance;
		}

		STATUS_CODE Initialize(IWindow* pWindow);

		VkInstance GetInstance() const;
		VkSurfaceKHR GetSurface() const;

		// Returns the API version, made through VK_MAKE_API_VERSION(X, Y, Z)
		u32 GetAPIVersion() const;

	private:

		CoreVk();
		~CoreVk();

		STATUS_CODE CreateInstance(bool enableValidationLayers);
		STATUS_CODE CreateSurface(IWindow* pWindow);

	private:

		VkInstance m_instance;
		VkSurfaceKHR m_surface;

		u32 m_apiVersion;
	};
}