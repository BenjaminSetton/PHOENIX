
#include <set>
#include <string>
#include <vector>

#include "render_device_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "core_vk.h"
#include "swap_chain_vk.h"
#include "utils/queue_family_indices.h"
#include "utils/swap_chain_helpers.h"

namespace PHX
{
	static const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		u32 extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		// Must be std::string so comparisons in erase() below work correctly
		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

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

	RenderDeviceVk::RenderDeviceVk(const RenderDeviceCreateInfo& ci) : m_logicalDevice(VK_NULL_HANDLE), m_physicalDevice(VK_NULL_HANDLE),
		physicalDeviceProperties(), physicalDeviceFeatures(), physicalDeviceMemoryProperties()
	{
		UNUSED(ci);

		const VkSurfaceKHR surface = CoreVk::Get().GetSurface();

		STATUS_CODE physicalRes = CreatePhysicalDevice(surface);
		STATUS_CODE logicalRes = CreateLogicalDevice(surface);

		if (physicalRes == STATUS_CODE::SUCCESS && logicalRes == STATUS_CODE::SUCCESS)
		{
			LogInfo("Successfully constructed Vk device!");
		}
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

	STATUS_CODE RenderDeviceVk::CreatePhysicalDevice(VkSurfaceKHR surface)
	{
		VkInstance instance = CoreVk::Get().GetInstance();

		u32 deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			ASSERT_ALWAYS("Failed to find physical device with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, surface))
			{
				vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);
				vkGetPhysicalDeviceFeatures(device, &physicalDeviceFeatures);
				vkGetPhysicalDeviceMemoryProperties(device, &physicalDeviceMemoryProperties);

				LogInfo("Using physical device: '%s'", physicalDeviceProperties.deviceName);
				m_physicalDevice = device;
				return STATUS_CODE::SUCCESS;
			}
		}

		LogError("Failed to find a suitable physical device!");
		return STATUS_CODE::ERR;
	}

	STATUS_CODE RenderDeviceVk::CreateLogicalDevice(VkSurfaceKHR surface)
	{
		VkPhysicalDevice physicalDevice = GetPhysicalDevice();

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		if (!indices.IsComplete())
		{
			LogError("Failed to create logical device because the queue family indices are incomplete!");
			return STATUS_CODE::ERR;
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
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS)
		{
			LogError("Failed to create the logical device!");
			return STATUS_CODE::ERR;
		}

		// Get the queues from the logical device
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::GRAPHICS), 0, &m_queues[QUEUE_TYPE::GRAPHICS]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::COMPUTE ), 0, &m_queues[QUEUE_TYPE::COMPUTE ]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::TRANSFER), 0, &m_queues[QUEUE_TYPE::TRANSFER]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::PRESENT ), 0, &m_queues[QUEUE_TYPE::PRESENT ]);

		return STATUS_CODE::SUCCESS;
	}
}

