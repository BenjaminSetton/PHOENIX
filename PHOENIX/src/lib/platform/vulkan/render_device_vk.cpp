
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

#include "acceleration_structure_vk.h"
#include "buffer_vk.h"
#include "core/handle/handle_utils.h"
#include "core_vk.h"
#include "device_context_vk.h"
#include "pipeline_vk.h"
#include "render_graph_vk.h"
#include "shader_vk.h"
#include "swap_chain_vk.h"
#include "texture_vk.h"
#include "uniform_vk.h"
#include "utils/logger.h"
#include "utils/queue_family_indices.h"
#include "utils/swap_chain_helpers.h"

namespace PHX
{
	static const std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	static const std::vector<const char*> rayTracingExtensions =
	{
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
		VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
		VK_KHR_SPIRV_1_4_EXTENSION_NAME
	};

	static bool SupportsAllExtensions(VkPhysicalDevice device, const std::vector<const char*>& extensions)
	{
		u32 extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		// Must be std::string so comparisons in erase() below work correctly
		std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	static bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{

		return SupportsAllExtensions(device, deviceExtensions);
	}

	static bool CheckRayTracingExtensionSupport(VkPhysicalDevice device)
	{
		if (!SupportsAllExtensions(device, rayTracingExtensions))
		{
			return false;
		}

		VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
		bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
		asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		asFeatures.pNext = &bdaFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpFeatures{};
		rtpFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtpFeatures.pNext = &asFeatures;

		VkPhysicalDeviceFeatures2 features2{};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &rtpFeatures;

		vkGetPhysicalDeviceFeatures2(device, &features2);

		return (bdaFeatures.bufferDeviceAddress && asFeatures.accelerationStructure && rtpFeatures.rayTracingPipeline);
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
		m_physicalDeviceProperties(), m_physicalDeviceFeatures(), m_physicalDeviceMemoryProperties(), m_rayTracingPipelineProperties(), m_descriptorPool(VK_NULL_HANDLE),
		m_rayTracingSupported(false), m_pfnCreateRayTracingPipelines(nullptr), m_pfnGetRayTracingShaderGroupHandles(nullptr), m_pfnGetBufferDeviceAddress(nullptr), m_pfnCmdTraceRays(nullptr),
		m_pfnCreateAccelerationStructure(nullptr), m_pfnDestroyAccelerationStructure(nullptr), m_pfnGetAccelerationStructureBuildSizes(nullptr), m_pfnGetAccelerationStructureDeviceAddress(nullptr), m_pfnCmdBuildAccelerationStructures(nullptr),
		m_textures(), m_buffers(), m_uniformCollections(), m_deviceContexts(), m_shaders(), m_swapChains(), m_renderGraphs(), m_accelerationStructures()
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;
		const VkSurfaceKHR surface = CoreVk::Get().GetSurface();

		res = CreatePhysicalDevice(surface);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		res = CreateLogicalDevice(surface);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		res = CreateVMAAllocator();
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		res = AllocateDescriptorPool();
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		res = AllocateCommandPools();
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		res = AllocateSyncObjects(ci.framesInFlight);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_framebufferCache = new FramebufferCache();
		m_renderPassCache = new RenderPassCache(this);
		m_pipelineCache = new PipelineCache(this);

		m_framesInFlight = ci.framesInFlight;

		LogInfo("Successfully constructed Vk device!");
	}

	RenderDeviceVk::~RenderDeviceVk()
	{
		vkDeviceWaitIdle(m_logicalDevice);

		SAFE_DEL(m_pipelineCache);
		SAFE_DEL(m_renderPassCache);
		SAFE_DEL(m_framebufferCache);

		// Destroy sync objects
		for (u32 i = 0; i < m_framesInFlight; i++)
		{
			vkDestroyFence(m_logicalDevice, m_inFlightFences[i], nullptr);
			vkDestroySemaphore(m_logicalDevice, m_imageAvailableSemaphores[i], nullptr);
		}
		m_inFlightFences.clear();
		m_imageAvailableSemaphores.clear();

		// Destroy command pools
		for (auto& iter : m_commandPools)
		{
			VkCommandPool& pool = iter.second;
			vkDestroyCommandPool(m_logicalDevice, pool, nullptr);
		}
		m_commandPools.clear();

		// Delete resources
		m_textures.DeleteAll();
		m_buffers.DeleteAll();
		m_uniformCollections.DeleteAll();
		m_deviceContexts.DeleteAll();
		m_shaders.DeleteAll();
		m_swapChains.DeleteAll();
		m_renderGraphs.DeleteAll();
		
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

	u32 RenderDeviceVk::GetFramesInFlight() const
	{
		return m_framesInFlight;
	}

	bool RenderDeviceVk::IsRayTracingSupported() const
	{
		return m_rayTracingSupported;
	}

	STATUS_CODE RenderDeviceVk::AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& handle)
	{
		BufferVk* pBuffer = new BufferVk(this, createInfo);
		if (pBuffer == nullptr)
		{
			LogError("Failed to allocate buffer. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_buffers, pBuffer, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& handle)
	{
		TextureVk* pTexture = new TextureVk(this, baseCreateInfo, viewCreateInfo, samplerCreateInfo);
		if (pTexture == nullptr)
		{
			LogError("Failed to allocate texture. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_textures, pTexture, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateSwapchainTexture(const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView, TextureHandle& handle)
	{
		TextureVk* pTexture = new TextureVk(this, baseCreateInfo, imageView);
		if (pTexture == nullptr)
		{
			LogError("Failed to allocate swapchain texture. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_textures, pTexture, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& handle)
	{
		UniformCollectionVk* pUniformCollection = new UniformCollectionVk(this, createInfo);
		if (pUniformCollection == nullptr)
		{
			LogError("Failed to allocate uniform collection. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_uniformCollections, pUniformCollection, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateRenderGraph(RenderGraphHandle& handle)
	{
		if (m_renderGraphs.Size() >= 1)
		{
			LogError("Cannot allocate more than one render graph!");
			return STATUS_CODE::ERR_API;
		}

		RenderGraphVk* pRenderGraph = new RenderGraphVk(this);
		if (pRenderGraph == nullptr)
		{
			LogError("Failed to allocate render graph. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_renderGraphs, pRenderGraph, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& handle)
	{
		ShaderVk* pShader = new ShaderVk(this, createInfo);
		if (pShader == nullptr)
		{
			LogError("Failed to allocate shader. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_shaders, pShader, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& handle)
	{
		SwapChainVk* pSwapChain = new SwapChainVk(this, createInfo);
		if (pSwapChain == nullptr)
		{
			LogError("Failed to allocate swap chain. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_swapChains, pSwapChain, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& handle)
	{
		DeviceContextVk* pContext = new DeviceContextVk(this, createInfo);
		if (pContext == nullptr)
		{
			LogError("Failed to allocate device context. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_deviceContexts, pContext, this, handle);
	}

	STATUS_CODE RenderDeviceVk::AllocateAccelerationStructure(const AccelerationStructureCreateInfo& createInfo, AccelerationStructureHandle& handle)
	{
		AccelerationStructureVk* pAccelerationStructure = new AccelerationStructureVk(this, createInfo);
		if (pAccelerationStructure == nullptr)
		{
			LogError("Failed to allocate acceleration structure. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		return HANDLE_UTILS::AllocateHandle(m_accelerationStructures, pAccelerationStructure, this, handle);
	}

	AccelerationStructureVk* RenderDeviceVk::GetAccelerationStructure(const AccelerationStructureHandle& handle)
	{
		return m_accelerationStructures.Resolve(handle.GetIndex());
	}

	void* RenderDeviceVk::ResolveHandle(const Handle& handle)
	{
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::BUFFER:                 return m_buffers.Resolve(handle.GetIndex());
		case HANDLE_TYPE::TEXTURE:                return m_textures.Resolve(handle.GetIndex());
		case HANDLE_TYPE::UNIFORM:                return m_uniformCollections.Resolve(handle.GetIndex());
		case HANDLE_TYPE::DEVICE_CONTEXT:         return m_deviceContexts.Resolve(handle.GetIndex());
		case HANDLE_TYPE::SHADER:                 return m_shaders.Resolve(handle.GetIndex());
		case HANDLE_TYPE::SWAP_CHAIN:             return m_swapChains.Resolve(handle.GetIndex());
		case HANDLE_TYPE::RENDER_GRAPH:           return m_renderGraphs.Resolve(handle.GetIndex());
		case HANDLE_TYPE::ACCELERATION_STRUCTURE: return m_accelerationStructures.Resolve(handle.GetIndex());
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to resolve handle. Unhandled type!");
		return nullptr;
	}

	void RenderDeviceVk::IncrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE handleType = handle.GetType();
		switch (handleType)
		{
		case HANDLE_TYPE::BUFFER:                 m_buffers.IncrementRefCount(handle.GetIndex());            	 break;
		case HANDLE_TYPE::TEXTURE:                m_textures.IncrementRefCount(handle.GetIndex());           	 break;
		case HANDLE_TYPE::UNIFORM:                m_uniformCollections.IncrementRefCount(handle.GetIndex()); 	 break;
		case HANDLE_TYPE::DEVICE_CONTEXT:         m_deviceContexts.IncrementRefCount(handle.GetIndex());     	 break;
		case HANDLE_TYPE::RENDER_GRAPH:           m_renderGraphs.IncrementRefCount(handle.GetIndex());       	 break;
		case HANDLE_TYPE::SHADER:                 m_shaders.IncrementRefCount(handle.GetIndex());            	 break;
		case HANDLE_TYPE::SWAP_CHAIN:             m_swapChains.IncrementRefCount(handle.GetIndex());         break;
		case HANDLE_TYPE::ACCELERATION_STRUCTURE: m_accelerationStructures.IncrementRefCount(handle.GetIndex()); break;
		default:
		{
			ASSERT_ALWAYS("Failed to increment ref count. Unrecognized handle type!");
			break;
		}
		}
	}

	void RenderDeviceVk::DecrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE handleType = handle.GetType();
		switch (handleType)
		{
		case HANDLE_TYPE::BUFFER:                 m_buffers.DecrementRefCount(handle.GetIndex());            	 break;
		case HANDLE_TYPE::TEXTURE:                m_textures.DecrementRefCount(handle.GetIndex());           	 break;
		case HANDLE_TYPE::UNIFORM:                m_uniformCollections.DecrementRefCount(handle.GetIndex()); 	 break;
		case HANDLE_TYPE::DEVICE_CONTEXT:         m_deviceContexts.DecrementRefCount(handle.GetIndex());     	 break;
		case HANDLE_TYPE::RENDER_GRAPH:           m_renderGraphs.DecrementRefCount(handle.GetIndex());       	 break;
		case HANDLE_TYPE::SHADER:                 m_shaders.DecrementRefCount(handle.GetIndex());            	 break;
		case HANDLE_TYPE::SWAP_CHAIN:             m_swapChains.DecrementRefCount(handle.GetIndex());         	 break;
		case HANDLE_TYPE::ACCELERATION_STRUCTURE: m_accelerationStructures.DecrementRefCount(handle.GetIndex()); break;
		default:
		{
			ASSERT_ALWAYS("Failed to decrement ref count. Unrecognized handle type!");
			break;
		}
		}
	}

	FramebufferVk* RenderDeviceVk::CreateFramebuffer(const FramebufferDescription& desc)
	{
		return m_framebufferCache->FindOrCreate(this, desc);
	}

	void RenderDeviceVk::DestroyFramebuffer(const FramebufferDescription& desc)
	{
		m_framebufferCache->Delete(desc);
	}

	FramebufferVk* RenderDeviceVk::GetFramebuffer(const FramebufferDescription& desc) const
	{
		return m_framebufferCache->Find(desc);
	}

	VkRenderPass RenderDeviceVk::GetOrCreateRenderPass(const RenderPassDescription& desc)
	{
		return m_renderPassCache->GetOrCreate(this, desc);
	}

	void RenderDeviceVk::DestroyRenderPass(const RenderPassDescription& desc)
	{
		m_renderPassCache->Delete(desc);
	}

	VkRenderPass RenderDeviceVk::GetRenderPass(const RenderPassDescription& desc) const
	{
		return m_renderPassCache->Find(desc);
	}

	PipelineVk* RenderDeviceVk::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, VkRenderPass renderPass)
	{
		PipelineVk* pipeline = m_pipelineCache->FindOrCreate(this, renderPass, desc);
		if (pipeline == nullptr)
		{
			ASSERT_ALWAYS("Failed to create graphics pipeline!");
		}

		return pipeline;
	}

	void RenderDeviceVk::DestroyGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		m_pipelineCache->Delete(desc);
	}

	PipelineVk* RenderDeviceVk::CreateComputePipeline(const ComputePipelineDesc& desc)
	{
		PipelineVk* pipeline = m_pipelineCache->FindOrCreate(this, desc);
		if (pipeline == nullptr)
		{
			ASSERT_ALWAYS("Failed to create compute pipeline!");
		}

		return pipeline;
	}

	void RenderDeviceVk::DestroyComputePipeline(const ComputePipelineDesc& desc)
	{
		m_pipelineCache->Delete(desc);
	}

	void RenderDeviceVk::InvalidateBackbufferFramebuffers()
	{
		std::vector<const FramebufferDescription*> m_invalidFramebufferDescs;
		m_invalidFramebufferDescs.reserve(5); // Should be plenty for any reasonable amount of backbuffer framebuffers

		// Find all backbuffer framebuffers in the cache
		const auto cacheBegin = m_framebufferCache->Begin();
		const auto cacheEnd = m_framebufferCache->End();
		for (auto iter = cacheBegin; iter != cacheEnd; iter++)
		{
			const FramebufferDescription& currDesc = iter->first;
			if (currDesc.isBackbuffer)
			{
				m_invalidFramebufferDescs.push_back(&currDesc);
			}
		}

		// Erase them
		for (auto& iter : m_invalidFramebufferDescs)
		{
			m_framebufferCache->Delete(*iter);
		}

		LogInfo("Invalidated %u backbuffer framebuffer objects!", m_invalidFramebufferDescs.size());
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

	VkSemaphore RenderDeviceVk::GetImageAvailableSemaphore(u32 index) const
	{
		if (index >= m_framesInFlight)
		{
			return VK_NULL_HANDLE;
		}

		return m_imageAvailableSemaphores[index];
	}

	VkFence RenderDeviceVk::GetInFlightFence(u32 index) const
	{
		if (index >= m_framesInFlight)
		{
			return VK_NULL_HANDLE;
		}

		return m_inFlightFences[index];
	}

	const VkPhysicalDeviceProperties& RenderDeviceVk::GetDeviceProperties() const
	{
		return m_physicalDeviceProperties;
	}

	const VkPhysicalDeviceFeatures& RenderDeviceVk::GetDeviceFeatures() const
	{
		return m_physicalDeviceFeatures;
	}

	const VkPhysicalDeviceMemoryProperties RenderDeviceVk::GetDeviceMemoryProperties() const
	{
		return m_physicalDeviceMemoryProperties;
	}

	STATUS_CODE RenderDeviceVk::CreateVMAAllocator()
	{
		VmaAllocatorCreateInfo info{};
		info.device = m_logicalDevice;
		info.physicalDevice = m_physicalDevice;
		info.instance = CoreVk::Get().GetInstance();
		info.flags = 0;
		info.vulkanApiVersion = CoreVk::Get().GetAPIVersion();
		info.pHeapSizeLimit = nullptr;
		info.pTypeExternalMemoryHandleTypes = nullptr;
		info.pVulkanFunctions = nullptr;
		// [OPTIONAL] info.preferredLargeHeapBlockSize
		// [OPTIONAL] info.pDeviceMemoryCallbacks

		if (m_rayTracingSupported)
		{
			info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		}

		VkResult res = vmaCreateAllocator(&info, &m_allocator);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create VMA allocator object. Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
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
				LogInfo("Using physical device: \"%s\"", m_physicalDeviceProperties.deviceName);

				m_rayTracingSupported = CheckRayTracingExtensionSupport(device);
				if (m_rayTracingSupported)
				{
					LogInfo("Ray tracing is supported on this device.");

					m_rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
					m_rayTracingPipelineProperties.pNext = nullptr;

					VkPhysicalDeviceProperties2 properties2{};
					properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
					properties2.pNext = &m_rayTracingPipelineProperties;
					vkGetPhysicalDeviceProperties2(device, &properties2);
				}
				else
				{
					LogWarning("Ray tracing is not supported on this device.");
				}
				
				m_physicalDevice = device;
				return STATUS_CODE::SUCCESS;
			}
		}

		LogError("Failed to find a suitable physical device!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderDeviceVk::CreateLogicalDevice(VkSurfaceKHR surface)
	{
		VkPhysicalDevice physicalDevice = GetPhysicalDevice();

		QueueFamilyIndices indices = FindQueueFamilies(physicalDevice, surface);
		if (!indices.IsComplete())
		{
			LogError("Failed to create logical device because the queue family indices are incomplete!");
			return STATUS_CODE::ERR_INTERNAL;
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

		VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
		bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
		asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		asFeatures.pNext = &bdaFeatures;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtpFeatures{};
		rtpFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		rtpFeatures.pNext = &asFeatures;

		VkPhysicalDeviceFeatures2 deviceFeatures{};
		deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures.pNext = nullptr;
		deviceFeatures.features.samplerAnisotropy = VK_TRUE;
		deviceFeatures.features.geometryShader = VK_TRUE;

		std::vector<const char*> enabledExtensions = deviceExtensions;
		if (m_rayTracingSupported)
		{
			enabledExtensions.insert(enabledExtensions.end(), rayTracingExtensions.begin(), rayTracingExtensions.end());

			bdaFeatures.bufferDeviceAddress = VK_TRUE;
			asFeatures.accelerationStructure = VK_TRUE;
			rtpFeatures.rayTracingPipeline = VK_TRUE;
			deviceFeatures.pNext = &rtpFeatures;
		}

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = &deviceFeatures;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = nullptr;
		createInfo.enabledExtensionCount = static_cast<u32>(enabledExtensions.size());
		createInfo.ppEnabledExtensionNames = enabledExtensions.data();

		VkResult res = vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_logicalDevice);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create the logical device! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Get the queues from the logical device
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::GRAPHICS), 0, &m_queues[QUEUE_TYPE::GRAPHICS]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::COMPUTE ), 0, &m_queues[QUEUE_TYPE::COMPUTE ]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::TRANSFER), 0, &m_queues[QUEUE_TYPE::TRANSFER]);
		vkGetDeviceQueue(m_logicalDevice, indices.GetIndex(QUEUE_TYPE::PRESENT ), 0, &m_queues[QUEUE_TYPE::PRESENT ]);

		m_queueFamilyIndices = indices;

		if (m_rayTracingSupported)
		{
			STATUS_CODE rtFnsRes = LoadRayTracingFunctions();
			if (rtFnsRes != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to load ray tracing function pointers!");
				return rtFnsRes;
			}
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateDescriptorPool()
	{
		// TODO - Fix these!
		// These are temporary so we can get this working. Completely random numbers
		const u32 numUniformBuffers = 50;
		const u32 numImageSamplers = 1024;
		const u32 numStorageBuffers = 50;
		const u32 numStorageImages = 50;
		const u32 numAccelerationStructures = 50;
		const u32 maxSets = 500;

		std::array<VkDescriptorPoolSize, 6> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = numUniformBuffers/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = numImageSamplers/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		poolSizes[2].descriptorCount = numImageSamplers/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[3].descriptorCount = numStorageBuffers/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		poolSizes[4].descriptorCount = numStorageImages/* * CONFIG::MaxFramesInFlight*/;
		poolSizes[5].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		poolSizes[5].descriptorCount = numAccelerationStructures/* * CONFIG::MaxFramesInFlight*/;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = maxSets;
		poolInfo.flags = 0;

		VkResult res = vkCreateDescriptorPool(GetLogicalDevice(), &poolInfo, nullptr, &m_descriptorPool);
		if (res != VK_SUCCESS) {
			LogError("Failed to create descriptor pool! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
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
			return STATUS_CODE::ERR_INTERNAL;
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
			LogError("Failed to create command pool of type %u! Got error: \"%s\"", static_cast<u32>(type), string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateSyncObjects(u32 framesInFlight)
	{
		VkResult res = VK_SUCCESS;

		VkSemaphoreCreateInfo semaphoreCI{};
		semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceCI{};
		fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		// Creates the fence on the signaled state so we don't block on this fence for
		// the first frame (when we don't have any previous frames to wait on)
		fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_imageAvailableSemaphores.resize(framesInFlight);
		m_inFlightFences.resize(framesInFlight);

		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			// IMAGE AVAILABLE SEMAPHORE
			res = vkCreateSemaphore(m_logicalDevice, &semaphoreCI, nullptr, &(m_imageAvailableSemaphores[i]));
			if (res != VK_SUCCESS)
			{
				LogError("Failed to create image available semaphore! Got error: \"%s\"", string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}

			// IN-FLIGHT FENCE
			res = vkCreateFence(m_logicalDevice, &fenceCI, nullptr, &(m_inFlightFences[i]));
			if (res != VK_SUCCESS)
			{
				LogError("Failed to create in-flight fence! Got error: \"%s\"", string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::LoadRayTracingFunctions()
	{
		m_pfnCreateRayTracingPipelines = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkCreateRayTracingPipelinesKHR");
		if (m_pfnCreateRayTracingPipelines == nullptr)
		{
			LogError("Failed to load vkCreateRayTracingPipelinesKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnGetRayTracingShaderGroupHandles = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
		if (m_pfnGetRayTracingShaderGroupHandles == nullptr)
		{
			LogError("Failed to load vkGetRayTracingShaderGroupHandlesKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkGetBufferDeviceAddressKHR");
		if (m_pfnGetBufferDeviceAddress == nullptr)
		{
			m_pfnGetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkGetBufferDeviceAddress");
			if (m_pfnGetBufferDeviceAddress == nullptr)
			{
				LogError("Failed to load vkGetBufferDeviceAddressKHR!");
				return STATUS_CODE::ERR_INTERNAL;
			}
		}

		m_pfnCmdTraceRays = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkCmdTraceRaysKHR");
		if (m_pfnCmdTraceRays == nullptr)
		{
			LogError("Failed to load vkCmdTraceRaysKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnCreateAccelerationStructure = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkCreateAccelerationStructureKHR");
		if (m_pfnCreateAccelerationStructure == nullptr)
		{
			LogError("Failed to load vkCreateAccelerationStructureKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnDestroyAccelerationStructure = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkDestroyAccelerationStructureKHR");
		if (m_pfnDestroyAccelerationStructure == nullptr)
		{
			LogError("Failed to load vkDestroyAccelerationStructureKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnGetAccelerationStructureBuildSizes = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
		if (m_pfnGetAccelerationStructureBuildSizes == nullptr)
		{
			LogError("Failed to load vkGetAccelerationStructureBuildSizesKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnGetAccelerationStructureDeviceAddress = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");
		if (m_pfnGetAccelerationStructureDeviceAddress == nullptr)
		{
			LogError("Failed to load vkGetAccelerationStructureDeviceAddressKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pfnCmdBuildAccelerationStructures = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(m_logicalDevice, "vkCmdBuildAccelerationStructuresKHR");
		if (m_pfnCmdBuildAccelerationStructures == nullptr)
		{
			LogError("Failed to load vkCmdBuildAccelerationStructuresKHR!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& RenderDeviceVk::GetRayTracingPipelineProperties() const
	{
		return m_rayTracingPipelineProperties;
	}

	VkResult RenderDeviceVk::CreateRayTracingPipelinesKHR(VkPipelineCache pipelineCache, const VkRayTracingPipelineCreateInfoKHR& createInfo, VkPipeline* pPipeline)
	{
		if (m_pfnCreateRayTracingPipelines == nullptr)
		{
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		return m_pfnCreateRayTracingPipelines(m_logicalDevice, VK_NULL_HANDLE, pipelineCache, 1, &createInfo, nullptr, pPipeline);
	}

	VkResult RenderDeviceVk::GetRayTracingShaderGroupHandlesKHR(VkPipeline pipeline, u32 firstGroup, u32 groupCount, u64 dataSize, void* pData)
	{
		if (m_pfnGetRayTracingShaderGroupHandles == nullptr)
		{
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		return m_pfnGetRayTracingShaderGroupHandles(m_logicalDevice, pipeline, firstGroup, groupCount, static_cast<size_t>(dataSize), pData);
	}

	VkDeviceAddress RenderDeviceVk::GetBufferDeviceAddressKHR(const VkBufferDeviceAddressInfo* pInfo)
	{
		if (m_pfnGetBufferDeviceAddress == nullptr)
		{
			return 0;
		}

		return m_pfnGetBufferDeviceAddress(m_logicalDevice, pInfo);
	}

	void RenderDeviceVk::CmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, u32 width, u32 height, u32 depth)
	{
		if (m_pfnCmdTraceRays == nullptr)
		{
			return;
		}

		m_pfnCmdTraceRays(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
	}

	VkResult RenderDeviceVk::CreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR* pCreateInfo, VkAccelerationStructureKHR* pAccelerationStructure)
	{
		if (m_pfnCreateAccelerationStructure == nullptr)
		{
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		return m_pfnCreateAccelerationStructure(m_logicalDevice, pCreateInfo, nullptr, pAccelerationStructure);
	}

	void RenderDeviceVk::DestroyAccelerationStructureKHR(VkAccelerationStructureKHR accelerationStructure)
	{
		if (m_pfnDestroyAccelerationStructure == nullptr)
		{
			return;
		}

		m_pfnDestroyAccelerationStructure(m_logicalDevice, accelerationStructure, nullptr);
	}

	void RenderDeviceVk::GetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const u32* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
	{
		if (m_pfnGetAccelerationStructureBuildSizes == nullptr)
		{
			return;
		}

		m_pfnGetAccelerationStructureBuildSizes(m_logicalDevice, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
	}

	VkDeviceAddress RenderDeviceVk::GetAccelerationStructureDeviceAddressKHR(const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
	{
		if (m_pfnGetAccelerationStructureDeviceAddress == nullptr)
		{
			return 0;
		}

		return m_pfnGetAccelerationStructureDeviceAddress(m_logicalDevice, pInfo);
	}

	void RenderDeviceVk::CmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, u32 infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
	{
		if (m_pfnCmdBuildAccelerationStructures == nullptr)
		{
			return;
		}

		m_pfnCmdBuildAccelerationStructures(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
	}

	PipelineVk* RenderDeviceVk::CreateRayTracingPipeline(const RayTracingPipelineDesc& desc)
	{
		PipelineVk* pipeline = m_pipelineCache->FindOrCreate(this, desc);
		if (pipeline == nullptr)
		{
			ASSERT_ALWAYS("Failed to create ray tracing pipeline!");
		}

		return pipeline;
	}

	void RenderDeviceVk::DestroyRayTracingPipeline(const RayTracingPipelineDesc& desc)
	{
		m_pipelineCache->Delete(desc);
	}
}

