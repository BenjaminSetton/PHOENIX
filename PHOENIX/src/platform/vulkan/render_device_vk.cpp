
#include <vector>

#include "render_device_vk.h"

#include "../utils/logger.h"
#include "../utils/sanity.h"
#include "utils/queue_family_indices.h"

namespace PHX
{
	static std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	static std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		u32 formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		u32 presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	//static std::vector<const char*> GetRequiredExtensions()
	//{
	//	u32 glfwExtensionCount = 0;
	//	const char** glfwExtensions;
	//	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	//	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	//	if (enableValidationLayers)
	//	{
	//		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	//	}

	//	return extensions;
	//}

	static bool CheckValidationLayerSupport()
	{
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
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

	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		u32 layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
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

	static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails details = QuerySwapChainSupport(device, surface);
			swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}

	static void CreateVkInstance(bool enableValidation, VkInstance& out_vkInstance)
	{
		// Check that we support all requested validation layers
		if (enableValidation && !CheckValidationLayerSupport())
		{
			LogError("Validation layers were requested, but one or more is not supported!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "PHOENIX";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidation)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			//PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		std::vector<const char*> extensions; //= GetRequiredExtensions();

		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		createInfo.enabledLayerCount = 0;

		if (vkCreateInstance(&createInfo, nullptr, &out_vkInstance) != VK_SUCCESS)
		{
			ASSERT_ALWAYS("Failed to create Vulkan instance!");
		}
	}

	static void CreatePhysicalDevice(VkInstance vkInstance, VkSurfaceKHR surface, VkPhysicalDevice& out_vkPhysicalDevice)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			ASSERT_ALWAYS("Failed to find physical device with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, surface))
			{
				//LogInfo("Using physical device: '%s'", DeviceCache::Get().GetPhysicalDeviceProperties().deviceName);
				LogInfo("Found suitable physical device with Vulkan support!");
				return;
			}
		}

		ASSERT_ALWAYS("Failed to find a suitable physical device!");
	}

	//-----------------------------------------------------------------------------------//

	RenderDeviceVk::RenderDeviceVk(const RenderDeviceCreateInfo& ci) : m_vkInstance(VK_NULL_HANDLE), m_logicalDevice(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE)
	{
		const bool enableValidation = ci.validationLayerCount > 0;
		CreateVkInstance(enableValidation, m_vkInstance);
		//CreatePhysicalDevice(m_vkInstance, m_surface, m_physicalDevice);

		LogInfo("Constructed Vk device!");
	}

	RenderDeviceVk::~RenderDeviceVk()
	{
		LogInfo("Destructed Vk device!");
	}

	bool RenderDeviceVk::AllocateBuffer()
	{
		LogInfo("Allocated buffer!");
		return true; 
	}

	bool RenderDeviceVk::AllocateCommandBuffer()
	{
		LogInfo("Allocated command buffer!"); 
		return true; 
	}

	bool RenderDeviceVk::AllocateTexture()
	{
		LogInfo("Allocated texture!");
		return true;
	}

	bool RenderDeviceVk::AllocateShader()
	{
		LogInfo("Allocated shader!");
		return true;
	}
}

