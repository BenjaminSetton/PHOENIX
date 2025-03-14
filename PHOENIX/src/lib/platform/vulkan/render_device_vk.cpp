
#include <array>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

// SILENCE VMA WARNINGS:
// C4100 - unreferenced formal parameter
// C4127 - conditional expression is constant
// C4189 - local variable is initialized but not referenced
// C4267 - conversion from 'size_t' to 'uint32_t', possible loss of data
// C4324 - structure was padded dueto alignment specifier
// C4505 - unreferenced function with internal linkage has been removed
#pragma warning(push)
#pragma warning(disable : 4100 4127 4189 4267 4324 4505)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)

#include "render_device_vk.h"

#include "buffer_vk.h"
#include "core_vk.h"
#include "device_context_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "shader_vk.h"
#include "swap_chain_vk.h"
#include "texture_vk.h"
#include "uniform_vk.h"
#include "utils/logger.h"
#include "utils/queue_family_indices.h"
#include "utils/sanity.h"
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
		m_physicalDeviceProperties(), m_physicalDeviceFeatures(), m_physicalDeviceMemoryProperties(), m_descriptorPool(VK_NULL_HANDLE)
	{
		UNUSED(ci);
		LogWarning("Ignoring render device create info!");

		STATUS_CODE res = STATUS_CODE::ERR;
		const VkSurfaceKHR surface = CoreVk::Get().GetSurface();

		res = CreatePhysicalDevice(surface);
		if (res == STATUS_CODE::ERR)
		{
			return;
		}

		res = CreateLogicalDevice(surface);
		if (res == STATUS_CODE::ERR)
		{
			return;
		}

		res = CreateVMAAllocator();
		if (res == STATUS_CODE::ERR)
		{
			return;
		}

		res = AllocateDescriptorPool();
		if (res == STATUS_CODE::ERR)
		{
			return;
		}

		res = AllocateCommandPools();
		if (res == STATUS_CODE::ERR)
		{
			return;
		}

		LogInfo("Successfully constructed Vk device!");
	}

	RenderDeviceVk::~RenderDeviceVk()
	{
		vkDeviceWaitIdle(m_logicalDevice);

		// Destroy command pools
		for (auto& iter : m_commandPools)
		{
			VkCommandPool& pool = iter.second;
			vkDestroyCommandPool(m_logicalDevice, pool, nullptr);
		}
		m_commandPools.clear();
		
		// Destroy descriptor pool
		vkDestroyDescriptorPool(m_logicalDevice, m_descriptorPool, nullptr);

		vmaDestroyAllocator(m_allocator);
		vkDestroyDevice(m_logicalDevice, nullptr);

		LogInfo("Destructed Vk device!");
	}

	const char* RenderDeviceVk::GetDeviceName() const 
	{
		return m_physicalDeviceProperties.deviceName;
	}

	STATUS_CODE RenderDeviceVk::AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, IDeviceContext** out_deviceContext)
	{
		*out_deviceContext = new DeviceContextVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateBuffer(const BufferCreateInfo& createInfo, IBuffer** out_buffer)
	{
		*out_buffer = new BufferVk(this, createInfo);
		return STATUS_CODE::SUCCESS; 
	}

	STATUS_CODE RenderDeviceVk::AllocateFramebuffer(const FramebufferCreateInfo& createInfo, IFramebuffer** out_framebuffer)
	{
		*out_framebuffer = new FramebufferVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, ITexture** out_texture)
	{
		*out_texture = new TextureVk(this, baseCreateInfo, viewCreateInfo, samplerCreateInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)
	{
		*out_shader = new ShaderVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateGraphicsPipeline(const GraphicsPipelineCreateInfo& createInfo, IPipeline** out_pipeline)
	{
		*out_pipeline = new PipelineVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateComputePipeline(const ComputePipelineCreateInfo& createInfo, IPipeline** out_pipeline)
	{
		*out_pipeline = new PipelineVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, IUniformCollection** out_uniformCollection)
	{
		*out_uniformCollection = new UniformCollectionVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	void RenderDeviceVk::DeallocateDeviceContext(IDeviceContext** pDeviceContext)
	{
		SAFE_DEL(*pDeviceContext);
	}

	void RenderDeviceVk::DeallocateBuffer(IBuffer** pBuffer)
	{
		SAFE_DEL(*pBuffer);
	}

	void RenderDeviceVk::DeallocateFramebuffer(IFramebuffer** pFramebuffer)
	{
		SAFE_DEL(*pFramebuffer);
	}

	void RenderDeviceVk::DeallocateTexture(ITexture** pTexture)
	{
		SAFE_DEL(*pTexture);
	}

	void RenderDeviceVk::DeallocateShader(IShader** pShader)
	{
		SAFE_DEL(*pShader);
	}

	void RenderDeviceVk::DeallocatePipeline(IPipeline** pPipeline)
	{
		SAFE_DEL(*pPipeline);
	}

	void RenderDeviceVk::DeallocateUniformCollection(IUniformCollection** pUniformCollection)
	{
		SAFE_DEL(*pUniformCollection);
	}

	VkDevice RenderDeviceVk::GetLogicalDevice() const
	{
		return m_logicalDevice;
	}

	VkPhysicalDevice RenderDeviceVk::GetPhysicalDevice() const 
	{
		return m_physicalDevice;
	}

	VmaAllocator RenderDeviceVk::GetAllocator() const
	{
		return m_allocator;
	}

	VkDescriptorPool RenderDeviceVk::GetDescriptorPool() const
	{
		return m_descriptorPool;
	}

	VkCommandPool RenderDeviceVk::GetCommandPool(QUEUE_TYPE type) const
	{
		auto iter = m_commandPools.find(type);
		if (iter == m_commandPools.end())
		{
			return VK_NULL_HANDLE;
		}

		return iter->second;
	}

	VkQueue RenderDeviceVk::GetQueue(QUEUE_TYPE type) const
	{
		auto iter = m_queues.find(type);
		if (iter == m_queues.end())
		{
			return VK_NULL_HANDLE;
		}

		return iter->second;
	}

	STATUS_CODE RenderDeviceVk::CreateVMAAllocator()
	{
		VmaAllocatorCreateInfo info{};
		info.device = m_logicalDevice;
		info.physicalDevice = m_physicalDevice;
		info.instance = CoreVk::Get().GetInstance();
		info.flags = 0; // TODO - can this be 0??
		info.vulkanApiVersion = CoreVk::Get().GetAPIVersion();
		info.pHeapSizeLimit = nullptr;
		info.pTypeExternalMemoryHandleTypes = nullptr;
		info.pVulkanFunctions = nullptr;
		// [OPTIONAL] info.preferredLargeHeapBlockSize
		// [OPTIONAL] info.pDeviceMemoryCallbacks

		VkResult res = vmaCreateAllocator(&info, &m_allocator);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create VMA allocator object. Got result: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
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
				vkGetPhysicalDeviceProperties(device, &m_physicalDeviceProperties);
				vkGetPhysicalDeviceFeatures(device, &m_physicalDeviceFeatures);
				vkGetPhysicalDeviceMemoryProperties(device, &m_physicalDeviceMemoryProperties);

				LogInfo("Using physical device: '%s'", m_physicalDeviceProperties.deviceName);
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

		VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_logicalDevice);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create the logical device! Got result: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		// Get the queues from the logical device
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::GRAPHICS), 0, &m_queues[QUEUE_TYPE::GRAPHICS]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::COMPUTE ), 0, &m_queues[QUEUE_TYPE::COMPUTE ]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::TRANSFER), 0, &m_queues[QUEUE_TYPE::TRANSFER]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::PRESENT ), 0, &m_queues[QUEUE_TYPE::PRESENT ]);

		m_queueFamilyIndices = indices;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateDescriptorPool()
	{
		// TODO - Fix these!
		// These are temporary so we can get this working. Completely random numbers
		const u32 numUniformBuffers = 50;
		const u32 numImageSamplers = 50;
		const u32 maxSets = 500;

		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = numUniformBuffers/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = numImageSamplers/* * CONFIG::MaxFramesInFlight*/;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = maxSets;
		poolInfo.flags = 0;

		VkResult res = vkCreateDescriptorPool(GetLogicalDevice(), &poolInfo, nullptr, &m_descriptorPool);
		if (res != VK_SUCCESS) {
			LogError("Failed to create descriptor pool! Got result: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateCommandPools()
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		res = AllocateCommandPool_Helper(QUEUE_TYPE::GRAPHICS, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		res = AllocateCommandPool_Helper(QUEUE_TYPE::COMPUTE, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		res = AllocateCommandPool_Helper(QUEUE_TYPE::TRANSFER, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		return res;
	}

	STATUS_CODE RenderDeviceVk::AllocateCommandPool_Helper(QUEUE_TYPE type, VkCommandPoolCreateFlags flags)
	{
		u32 queueFamilyIndex = m_queueFamilyIndices.GetIndex(type);
		if (!m_queueFamilyIndices.IsValid(queueFamilyIndex))
		{
			LogError("Failed to allocate command pool of type %u! Queue family index is not valid", static_cast<u32>(type));
			return STATUS_CODE::ERR;
		}

		// Allocate the pool object in the map
		m_commandPools.insert({type, VK_NULL_HANDLE});

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = flags;
		poolInfo.queueFamilyIndex = queueFamilyIndex;

		VkResult res = vkCreateCommandPool(GetLogicalDevice(), &poolInfo, nullptr, &m_commandPools[type]);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create command pool of type %u! Got result: %s", static_cast<u32>(type), string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}
}

