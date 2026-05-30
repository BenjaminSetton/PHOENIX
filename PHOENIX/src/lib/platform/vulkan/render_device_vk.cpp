
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
#include "core/handle/handle_accessor.h"
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

		// Clear resources
		m_textures.clear();
		m_buffers.clear();
		m_uniformCollections.clear();
		m_deviceContexts.clear();
		
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

	STATUS_CODE RenderDeviceVk::AllocateSwapChain(const SwapChainCreateInfo& createInfo, ISwapChain** out_swapChain)
	{
		*out_swapChain = new SwapChainVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateRenderGraph(IRenderGraph** out_renderGraph)
	{
		*out_renderGraph = new RenderGraphVk(this);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& handle)
	{
		BufferVk* pBuffer = new BufferVk(this, createInfo);
		if (pBuffer == nullptr)
		{
			LogError("Failed to allocate buffer. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		m_buffers.push_back(pBuffer);
		
		pBuffer->IncrementRefCount();
		HandleAccessor::PopulateHandle(handle, this, static_cast<u32>(m_buffers.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& texture)
	{
		TextureVk* pTexture = new TextureVk(this, baseCreateInfo, viewCreateInfo, samplerCreateInfo);
		if (pTexture == nullptr)
		{
			LogError("Failed to allocate texture. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		m_textures.push_back(pTexture);

		pTexture->IncrementRefCount();
		HandleAccessor::PopulateHandle(texture, this, static_cast<u32>(m_textures.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateSwapchainTexture(const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView, TextureHandle& handle)
	{
		TextureVk* pTexture = new TextureVk(this, baseCreateInfo, imageView);
		if (pTexture == nullptr)
		{
			LogError("Failed to allocate swapchain texture. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		m_textures.push_back(pTexture);

		pTexture->IncrementRefCount();
		HandleAccessor::PopulateHandle(handle, this, static_cast<u32>(m_textures.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& uniformCollection)
	{
		UniformCollectionVk* pUniformCollection = new UniformCollectionVk(this, createInfo);
		if (pUniformCollection == nullptr)
		{
			LogError("Failed to allocate uniform collection. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		m_uniformCollections.push_back(pUniformCollection);

		pUniformCollection->IncrementRefCount();
		HandleAccessor::PopulateHandle(uniformCollection, this, static_cast<u32>(m_uniformCollections.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderDeviceVk::AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader)
	{
		*out_shader = new ShaderVk(this, createInfo);
		return STATUS_CODE::SUCCESS;
	}

	void RenderDeviceVk::DeallocateSwapChain(ISwapChain** pSwapChain)
	{
		SAFE_DEL(*pSwapChain);
	}

	void RenderDeviceVk::DeallocateRenderGraph(IRenderGraph** pRenderGraph)
	{
		SAFE_DEL(*pRenderGraph);
	}

	void RenderDeviceVk::DeallocateResource(const Handle& handle)
	{
		HANDLE_TYPE handleType = HandleAccessor::GetType(handle);
		switch (handleType)
		{
		case HANDLE_TYPE::BUFFER:
		{
			DeallocateResource_Helper<BufferHandle, BufferVk>(m_buffers, handle);
			break;
		}
		case HANDLE_TYPE::TEXTURE:
		{
			DeallocateResource_Helper<TextureHandle, TextureVk>(m_textures, handle);
			break;
		}
		case HANDLE_TYPE::UNIFORM:
		{
			DeallocateResource_Helper<UniformCollectionHandle, UniformCollectionVk>(m_uniformCollections, handle);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to deallocate resource. Unrecognized handle type!");
			break;
		}
		}
	}

	void RenderDeviceVk::DeallocateShader(IShader** pShader)
	{
		SAFE_DEL(*pShader);
	}

	u32 RenderDeviceVk::GetFramesInFlight() const
	{
		return m_framesInFlight;
	}

	STATUS_CODE RenderDeviceVk::AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& deviceContext)
	{
		DeviceContextVk* pContext = new DeviceContextVk(this, createInfo);
		if (pContext == nullptr)
		{
			LogError("Failed to allocate device context. Memory allocation failed!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		m_deviceContexts.push_back(pContext);

		pContext->IncrementRefCount();
		HandleAccessor::PopulateHandle(deviceContext, this, static_cast<u32>(m_deviceContexts.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
	}

	ITexture* RenderDeviceVk::ResolveHandle(const TextureHandle& handle)
	{
		const u32 index = HandleAccessor::GetIndex(handle);
		if (index >= static_cast<u32>(m_textures.size()))
		{
			return nullptr;
		}
		// TODO - Check generation

		return m_textures[index];
	}

	IBuffer* RenderDeviceVk::ResolveHandle(const BufferHandle& handle)
	{
		const u32 index = HandleAccessor::GetIndex(handle);
		if (index >= static_cast<u32>(m_buffers.size()))
		{
			return nullptr;
		}
		// TODO - Check generation

		return m_buffers[index];
	}

	IUniformCollection* RenderDeviceVk::ResolveHandle(const UniformCollectionHandle& handle)
	{
		const u32 index = HandleAccessor::GetIndex(handle);
		if (index >= static_cast<u32>(m_uniformCollections.size()))
		{
			return nullptr;
		}
		// TODO - Check generation

		return m_uniformCollections[index];
	}

	IDeviceContext* RenderDeviceVk::ResolveHandle(const DeviceContextHandle& handle)
	{
		const u32 index = HandleAccessor::GetIndex(handle);
		if (index >= static_cast<u32>(m_deviceContexts.size()))
		{
			return nullptr;
		}
		// TODO - Check generation

		return m_deviceContexts[index];
	}

	void RenderDeviceVk::IncrementRefCount(const Handle& handle)
	{
		HANDLE_TYPE handleType = HandleAccessor::GetType(handle);
		switch (handleType)
		{
		case HANDLE_TYPE::BUFFER:
		{
			IncrementRefCount_Helper<BufferHandle, IBuffer>(handle);
			break;
		}
		case HANDLE_TYPE::TEXTURE:
		{
			IncrementRefCount_Helper<TextureHandle, ITexture>(handle);
			break;
		}
		case HANDLE_TYPE::UNIFORM:
		{
			IncrementRefCount_Helper<UniformCollectionHandle, IUniformCollection>(handle);
			break;
		}
		case HANDLE_TYPE::DEVICE_CONTEXT:
		{
			IncrementRefCount_Helper<DeviceContextHandle, IDeviceContext>(handle);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to increment ref count. Unrecognized handle type!");
		}
		}
	}

	void RenderDeviceVk::DecrementRefCount(const Handle& handle)
	{
		HANDLE_TYPE handleType = HandleAccessor::GetType(handle);
		switch (handleType)
		{
		case HANDLE_TYPE::BUFFER:
		{
			DecrementRefCount_Helper<BufferHandle, IBuffer>(handle);
			break;
		}
		case HANDLE_TYPE::TEXTURE:
		{
			DecrementRefCount_Helper<TextureHandle, ITexture>(handle);
			break;
		}
		case HANDLE_TYPE::UNIFORM:
		{
			DecrementRefCount_Helper<UniformCollectionHandle, IUniformCollection>(handle);
			break;
		}
		case HANDLE_TYPE::DEVICE_CONTEXT:
		{
			DecrementRefCount_Helper<DeviceContextHandle, IDeviceContext>(handle);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to decrement ref count. Unrecognized handle type!");
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

	VkRenderPass RenderDeviceVk::CreateRenderPass(const RenderPassDescription& desc)
	{
		return m_renderPassCache->FindOrCreate(this, desc);
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

	PipelineVk* RenderDeviceVk::GetGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		return m_pipelineCache->Find(desc);
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

	PipelineVk* RenderDeviceVk::GetComputePipeline(const ComputePipelineDesc& desc)
	{
		return m_pipelineCache->Find(desc);
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
			LogError("Failed to create the logical device! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
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
}

