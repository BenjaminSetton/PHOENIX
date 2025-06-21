
#include <glfw/glfw3.h> // glfwVulkanSupported, glfwGetRequiredInstanceExtensions
#include <vector>
#include <vulkan/vk_enum_string_helper.h> // string_VkResult

#include "core_vk.h"

#include "core/global_settings.h"
#include "utils/logger.h"
#include "utils/sanity.h"

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
	static VkBool32 OnValidationMessageReceived(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		UNUSED(messageTypes);
		UNUSED(pUserData);

		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		{
			LogInfo("%s", pCallbackData->pMessage);
			break;
		}
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		{
			LogWarning("%s", pCallbackData->pMessage);
			break;
		}
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		{
			LogError("%s", pCallbackData->pMessage);
			break;
		}
		}

		return true;
	}

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

	STATUS_CODE CoreVk::Initialize(IWindow* pWindow)
	{
		const Settings& settings = GetSettings();

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		res = CreateInstance(settings.enableValidation);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		res = CreateSurface(pWindow);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		LogInfo("Successfully initialized Vulkan version %u.%u.%u", VK_API_VERSION_MAJOR(m_apiVersion), VK_API_VERSION_MINOR(m_apiVersion), VK_API_VERSION_PATCH(m_apiVersion));

		return res;
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
		m_apiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	}

	CoreVk::~CoreVk()
	{
		DestroySurface();
		DestroyInstance();
	}

	STATUS_CODE CoreVk::CreateInstance(bool enableValidationLayers)
	{
		// Check that we support all requested validation layers
		if (enableValidationLayers && !CheckValidationLayerSupport())
		{
			LogError("Validation layers were requested, but one or more are not supported!");
			return STATUS_CODE::ERR_INTERNAL;
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
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(g_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = g_ValidationLayers.data();
		}

		std::vector<const char*> requiredExtensions = GetRequiredExtensions();

		if (enableValidationLayers)
		{
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		VkResult res = vkCreateInstance(&createInfo, nullptr, &m_instance);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create Vulkan instance! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Create debug messenger, if requested by client
		m_validationMessenger = VK_NULL_HANDLE;
		if (enableValidationLayers)
		{
			PFN_vkCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");

			VkDebugUtilsMessengerCreateInfoEXT validationMessengerCreateInfo{};
			validationMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			validationMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			validationMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
			validationMessengerCreateInfo.pfnUserCallback = OnValidationMessageReceived;
			validationMessengerCreateInfo.pUserData = nullptr; // Optional

			pfnCreateDebugUtilsMessengerEXT(m_instance, &validationMessengerCreateInfo, nullptr, &m_validationMessenger);
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CoreVk::CreateSurface(IWindow* pWindow)
	{
		if (pWindow == nullptr)
		{
			LogError("Failed to create surface! Window is null");
			return STATUS_CODE::ERR_API;
		}

#if defined(PHX_WINDOWS)
		WindowWin64* windowWin64 = static_cast<WindowWin64*>(pWindow);
		if (windowWin64 != nullptr)
		{
			VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
			surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			surfaceCreateInfo.hinstance = GetModuleHandle(NULL);
			surfaceCreateInfo.hwnd = reinterpret_cast<HWND>(windowWin64->GetNativeHandle());

			VkResult res = vkCreateWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to create win32 surface! Got error: \"%s\"", string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}
		}
#endif

		return STATUS_CODE::SUCCESS;
	}

	void CoreVk::DestroyInstance()
	{
		// Destroy debug utilities, if any
		if (m_validationMessenger != VK_NULL_HANDLE)
		{
			PFN_vkDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
			pfnDestroyDebugUtilsMessengerEXT(m_instance, m_validationMessenger, nullptr);
		}

		vkDestroyInstance(m_instance, nullptr);
	}

	void CoreVk::DestroySurface()
	{
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

}