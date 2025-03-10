
#include <glfw/glfw3.h> // glfwVulkanSupported, glfwGetRequiredInstanceExtensions
#include <vector>

#include "core_vk.h"

#include "../../utils/logger.h"

#if defined(PHX_WINDOWS)
#undef APIENTRY // Fix for "APIENTRY macro redefinition" warning. Windows.h defines this unconditionally, and glfw3.h defines it too
#include "../win64/window_win64.h"
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#else
#error Platform is not supported!
#endif

namespace PHX
{
	static const std::vector<const char*> g_ValidationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	static std::vector<const char*> GetRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		return extensions;
	}

	static bool CheckValidationLayerSupport()
	{
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : g_ValidationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}

		return true;
	}

	STATUS_CODE CoreVk::Initialize(bool enableValidationLayers, std::shared_ptr<IWindow> pWindow)
	{
		if (CreateInstance(enableValidationLayers) != STATUS_CODE::SUCCESS)
		{
			return STATUS_CODE::ERR;
		}

		if (CreateSurface(pWindow) != STATUS_CODE::SUCCESS)
		{
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	VkInstance CoreVk::GetInstance() const
	{
		return m_instance;
	}

	VkSurfaceKHR CoreVk::GetSurface() const
	{
		return m_surface;
	}

	u32 CoreVk::GetAPIVersion() const
	{
		return m_apiVersion;
	}

	CoreVk::CoreVk() : m_instance(VK_NULL_HANDLE), m_surface(VK_NULL_HANDLE), m_apiVersion(0)
	{
		// 1.0.0
		m_apiVersion = VK_API_VERSION_1_0;
	}

	CoreVk::~CoreVk()
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		vkDestroyInstance(m_instance, nullptr);
	}

	STATUS_CODE CoreVk::CreateInstance(bool enableValidationLayers)
	{
		// Check that we support all requested validation layers
		if (enableValidationLayers && !CheckValidationLayerSupport())
		{
			LogError("Validation layers were requested, but one or more is not supported!");
			return STATUS_CODE::ERR;
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "PHOENIX";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = m_apiVersion;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = g_ValidationLayers.data();

			//VkDebugUtilsMessengerCreateInfoEXT& debugUtilsCreateInfo;
			//debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			//debugUtilsCreateInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			//debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
			//debugUtilsCreateInfo.pfnUserCallback = debugCallback;
			//debugUtilsCreateInfo.pUserData = nullptr; // Optional

			//createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		std::vector<const char*> requiredExtensions = GetRequiredExtensions();

		if (enableValidationLayers)
		{
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		createInfo.enabledLayerCount = 0;

		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
		{
			LogError("Failed to create Vulkan instance!");
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CoreVk::CreateSurface(std::shared_ptr<IWindow> pWindow)
	{
		if (pWindow.get() == nullptr)
		{
			LogError("Failed to create surface. Window is null!");
			return STATUS_CODE::ERR;
		}

#if defined(PHX_WINDOWS)
		WindowWin64* windowWin64 = dynamic_cast<WindowWin64*>(pWindow.get());
		if (windowWin64 != nullptr)
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
			surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(windowWin64->GetNativeHandle());

			if (vkCreateWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface) != VK_SUCCESS)
			{
				LogError("Failed to create win32 surface!");
				return STATUS_CODE::ERR;
			}
		}
#endif

		return STATUS_CODE::SUCCESS;
	}

}