#pragma once

#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "core/handle/handle_list.h"
#include "framebuffer_vk.h"
#include "PHX/interface/render_device.h"
#include "PHX/types/queue_type.h"
#include "PHX/types/status_code.h"
#include "core/interface_types/render_device_interface.h"
#include "utils/framebuffer_cache.h"
#include "utils/pipeline_cache.h"
#include "utils/queue_family_indices.h"
#include "utils/render_pass_cache.h"
#include "utils/sanity.h"

namespace PHX
{
	// Forward declarations
	class AccelerationStructureVk;
	class BufferVk;
	class DeviceContextVk;
	class RenderGraphVk;
	class TextureVk;
	class UniformCollectionVk;
	class ShaderVk;
	class SwapChainVk;

	class RenderDeviceVk : public IRenderDevice
	{
	public:

		explicit RenderDeviceVk(const RenderDeviceCreateInfo& ci);
		~RenderDeviceVk() override;

		const char* GetDeviceName() const;
		u32 GetFramesInFlight() const override;
		bool IsRayTracingSupported() const override;
		
		// Allocations
		STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, BufferHandle& handle) override;
		STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, TextureHandle& handle) override;
		STATUS_CODE AllocateSwapchainTexture(const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView, TextureHandle& handle);
		STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, UniformCollectionHandle& handle) override;
		STATUS_CODE AllocateRenderGraph(RenderGraphHandle& handle) override;
		STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, ShaderHandle& handle) override;
		STATUS_CODE AllocateSwapChain(const SwapChainCreateInfo& createInfo, SwapChainHandle& handle) override;
		STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, DeviceContextHandle& handle) override;
		STATUS_CODE AllocateAccelerationStructure(const AccelerationStructureCreateInfo& createInfo, AccelerationStructureHandle& handle) override;

		// Handles
		void* ResolveHandle(const Handle& handle) override;
		void IncrementHandleRefCount(const Handle& handle) override;
		void DecrementHandleRefCount(const Handle& handle) override;

		// Cached creation calls - vulkan only
		FramebufferVk* CreateFramebuffer(const FramebufferDescription& desc);
		void DestroyFramebuffer(const FramebufferDescription& desc);
		FramebufferVk* GetFramebuffer(const FramebufferDescription& desc) const;
		
		VkRenderPass GetOrCreateRenderPass(const RenderPassDescription& desc);
		void DestroyRenderPass(const RenderPassDescription& desc);
		VkRenderPass GetRenderPass(const RenderPassDescription& desc) const;

		PipelineVk* CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, VkRenderPass renderPass);
		void DestroyGraphicsPipeline(const GraphicsPipelineDesc& desc);

		PipelineVk* CreateComputePipeline(const ComputePipelineDesc& desc);
		void DestroyComputePipeline(const ComputePipelineDesc& desc);

		PipelineVk* CreateRayTracingPipeline(const RayTracingPipelineDesc& desc);
		void DestroyRayTracingPipeline(const RayTracingPipelineDesc& desc);
		
		AccelerationStructureVk* GetAccelerationStructure(const AccelerationStructureHandle& handle);

		// Removes all framebuffer entries in the cache related to the backbuffer. 
		// This is used to clean up old framebuffers after a window resize, for example
		void InvalidateBackbufferFramebuffers();

		// Getters
		VkDevice GetLogicalDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;
		VmaAllocator GetAllocator() const;
		VkDescriptorPool GetDescriptorPool() const;
		VkCommandPool GetCommandPool(QUEUE_TYPE type) const;
		VkQueue GetQueue(QUEUE_TYPE type) const;
		VkSemaphore GetImageAvailableSemaphore(u32 index) const;
		VkFence GetInFlightFence(u32 index) const;

		// Device info
		const VkPhysicalDeviceProperties& GetDeviceProperties() const;
		const VkPhysicalDeviceFeatures& GetDeviceFeatures() const;
		const VkPhysicalDeviceMemoryProperties GetDeviceMemoryProperties() const;
		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRayTracingPipelineProperties() const;

		// Ray tracing Vulkan wrappers
		VkResult CreateRayTracingPipelinesKHR(VkPipelineCache pipelineCache, const VkRayTracingPipelineCreateInfoKHR& createInfo, VkPipeline* pPipeline);
		VkResult GetRayTracingShaderGroupHandlesKHR(VkPipeline pipeline, u32 firstGroup, u32 groupCount, size_t dataSize, void* pData);
		VkDeviceAddress GetBufferDeviceAddressKHR(const VkBufferDeviceAddressInfo* pInfo);
		void CmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, u32 width, u32 height, u32 depth);

		// Acceleration structure Vulkan wrappers
		VkResult CreateAccelerationStructureKHR(const VkAccelerationStructureCreateInfoKHR* pCreateInfo, VkAccelerationStructureKHR* pAccelerationStructure);
		void DestroyAccelerationStructureKHR(VkAccelerationStructureKHR accelerationStructure);
		void GetAccelerationStructureBuildSizesKHR(VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const u32* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo);
		VkDeviceAddress GetAccelerationStructureDeviceAddressKHR(const VkAccelerationStructureDeviceAddressInfoKHR* pInfo);
		void CmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, u32 infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos);

	private:

		STATUS_CODE CreateVMAAllocator();

		STATUS_CODE CreatePhysicalDevice(VkSurfaceKHR surface);
		STATUS_CODE CreateLogicalDevice(VkSurfaceKHR surface);

		STATUS_CODE AllocateDescriptorPool();

		STATUS_CODE AllocateCommandPools();
		STATUS_CODE AllocateCommandPool_Helper(QUEUE_TYPE type, VkCommandPoolCreateFlags flags);

		STATUS_CODE AllocateSyncObjects(u32 framesInFlight);

	private:

		VmaAllocator m_allocator;

		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;
		std::unordered_map<QUEUE_TYPE, VkQueue> m_queues;
		QueueFamilyIndices m_queueFamilyIndices;

		u32 m_framesInFlight;
		bool m_rayTracingSupported;

		// Physical device cache
		VkPhysicalDeviceProperties m_physicalDeviceProperties;
		VkPhysicalDeviceFeatures m_physicalDeviceFeatures;
		VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rayTracingPipelineProperties;

		// Ray tracing function pointers
		PFN_vkCreateRayTracingPipelinesKHR m_pfnCreateRayTracingPipelines;
		PFN_vkGetRayTracingShaderGroupHandlesKHR m_pfnGetRayTracingShaderGroupHandles;
		PFN_vkGetBufferDeviceAddressKHR m_pfnGetBufferDeviceAddress;
		PFN_vkCmdTraceRaysKHR m_pfnCmdTraceRays;

		// Acceleration structure function pointers
		PFN_vkCreateAccelerationStructureKHR m_pfnCreateAccelerationStructure;
		PFN_vkDestroyAccelerationStructureKHR m_pfnDestroyAccelerationStructure;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetAccelerationStructureBuildSizes;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetAccelerationStructureDeviceAddress;
		PFN_vkCmdBuildAccelerationStructuresKHR m_pfnCmdBuildAccelerationStructures;

		STATUS_CODE LoadRayTracingFunctions();

		// Descriptor pool
		VkDescriptorPool m_descriptorPool;

		// Command pools
		std::unordered_map<QUEUE_TYPE, VkCommandPool> m_commandPools;

		// Object caches
		FramebufferCache* m_framebufferCache;
		RenderPassCache* m_renderPassCache;
		PipelineCache* m_pipelineCache;

		// Sync objects
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkFence> m_inFlightFences;

		// Resource objects
		HandleList<TextureVk> m_textures;
		HandleList<BufferVk> m_buffers;
		HandleList<UniformCollectionVk> m_uniformCollections;
		HandleList<DeviceContextVk> m_deviceContexts;
		HandleList<ShaderVk> m_shaders;
		HandleList<SwapChainVk> m_swapChains; // Possibly support multiple windows?
		HandleList<RenderGraphVk> m_renderGraphs;
		HandleList<AccelerationStructureVk> m_accelerationStructures;
	};
}