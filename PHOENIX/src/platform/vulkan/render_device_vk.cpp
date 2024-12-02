
#include <set>
#include <vector>

#include "render_device_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "swap_chain_vk.h"
#include "utils/queue_family_indices.h"
#include "utils/swap_chain_helpers.h"

namespace PHX
{
	static const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

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

	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		u32 extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<const char*> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	static bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, surface);
		bool allExtensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (allExtensionsSupported)
		{
			SwapChainSupportDetails details = QuerySwapChainSupport(device, surface);
			swapChainAdequate = !details.formats.empty() && !details.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return (indices.IsComplete() && allExtensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy);
	}

	//-----------------------------------------------------------------------------------//

	RenderDeviceVk::RenderDeviceVk(const RenderDeviceCreateInfo& ci) : m_vkInstance(VK_NULL_HANDLE), m_logicalDevice(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE)
	{
		//const IWindow* window = dynamic_cast<SwapChainVk*>(ci.window);
		//ASSERT_PTR(swapChainVk);
		const bool enableValidation = ci.validationLayerCount > 0;
		//const VkSurfaceKHR surface = nullptr;// swapChainVk->GetSurface();

		CreateVkInstance(enableValidation);
		CreatePhysicalDevice(surface);
		CreateLogicalDevice(enableValidation, surface);

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

	VkDevice RenderDeviceVk::GetLogicalDevice() const
	{
		return m_logicalDevice;
	}

	VkPhysicalDevice RenderDeviceVk::GetPhysicalDevice() const 
	{
		return m_physicalDevice;
	}

	void RenderDeviceVk::CreatePhysicalDevice(VkSurfaceKHR surface)
	{
		u32 deviceCount = 0;
		vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			ASSERT_ALWAYS("Failed to find physical device with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, surface))
			{
				//LogInfo("Using physical device: '%s'", DeviceCache::Get().GetPhysicalDeviceProperties().deviceName);
				LogInfo("Found suitable physical device with Vulkan support!");
				m_physicalDevice = device;
				return;
			}
		}

		ASSERT_ALWAYS("Failed to find a suitable physical device!");
	}

	void RenderDeviceVk::CreateLogicalDevice(bool enableValidation, VkSurfaceKHR surface)
	{
		VkPhysicalDevice physicalDevice = GetPhysicalDevice();

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		if (!indices.IsComplete())
		{
			LogError("Failed to create logical device because the queue family indices are incomplete!");
			return;
		}

		LogInfo("Selected graphics queue from queue family at index %u", indices.GetIndex(QUEUE_TYPE::GRAPHICS));
		LogInfo("Selected compute queue from queue family at index %u" , indices.GetIndex(QUEUE_TYPE::COMPUTE ));
		LogInfo("Selected transfer queue from queue family at index %u", indices.GetIndex(QUEUE_TYPE::TRANSFER));
		LogInfo("Selected present queue from queue family at index %u" , indices.GetIndex(QUEUE_TYPE::PRESENT ));

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<u32> uniqueQueueFamilies = {
			indices.GetIndex(QUEUE_TYPE::GRAPHICS),
			indices.GetIndex(QUEUE_TYPE::COMPUTE),
			indices.GetIndex(QUEUE_TYPE::PRESENT),
			indices.GetIndex(QUEUE_TYPE::TRANSFER)
		};

		// TODO - Determine priority of the different queue types
		float queuePriority = 1.0f;
		for (u32 queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.geometryShader = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidation)
		{
			// We get a warning about using deprecated and ignored 'ppEnabledLayerNames', so I've commented these out.
			// It looks like validation layers work regardless...somehow...
			//createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			//createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS)
		{
			ASSERT_ALWAYS("Failed to create the logical device!");
		}

		// Get the queues from the logical device
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::GRAPHICS), 0, &m_queues[QUEUE_TYPE::GRAPHICS]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::COMPUTE ), 0, &m_queues[QUEUE_TYPE::COMPUTE ]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::TRANSFER), 0, &m_queues[QUEUE_TYPE::TRANSFER]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::PRESENT ), 0, &m_queues[QUEUE_TYPE::PRESENT ]);
	}
}

