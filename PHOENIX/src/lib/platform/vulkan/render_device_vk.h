#pragma once

#include <unordered_map>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "framebuffer_vk.h"
#include "PHX/interface/render_device.h"
#include "PHX/types/queue_type.h"
#include "PHX/types/status_code.h"
#include "utils/framebuffer_cache.h"
#include "utils/pipeline_cache.h"
#include "utils/queue_family_indices.h"
#include "utils/render_pass_cache.h"

namespace PHX
{
	class RenderDeviceVk : public IRenderDevice
	{
	public:

		explicit RenderDeviceVk(const RenderDeviceCreateInfo& ci);
		~RenderDeviceVk() override;

		const char* GetDeviceName() const;

		STATUS_CODE AllocateDeviceContext(const DeviceContextCreateInfo& createInfo, IDeviceContext** out_deviceContext) override; // TODO - REMOVE
		STATUS_CODE AllocateRenderGraph(IRenderGraph** out_renderGraph) override;
		STATUS_CODE AllocateBuffer(const BufferCreateInfo& createInfo, IBuffer** out_buffer) override;
		STATUS_CODE AllocateTexture(const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo, ITexture** out_texture) override;
		STATUS_CODE AllocateShader(const ShaderCreateInfo& createInfo, IShader** out_shader) override;
		STATUS_CODE AllocateUniformCollection(const UniformCollectionCreateInfo& createInfo, IUniformCollection** out_uniformCollection) override;

		void DeallocateDeviceContext(IDeviceContext** pDeviceContext) override;
		void DeallocateBuffer(IBuffer** pBuffer) override;
		void DeallocateTexture(ITexture** pTexture) override;
		void DeallocateShader(IShader** pShader) override;
		void DeallocateUniformCollection(IUniformCollection** pUniformCollection) override;

		// Cached creation calls - vulkan only
		FramebufferVk* CreateFramebuffer(const FramebufferDescription& desc);
		void DestroyFramebuffer(const FramebufferDescription& desc);
		FramebufferVk* GetFramebuffer(const FramebufferDescription& desc) const;
		
		VkRenderPass CreateRenderPass(const RenderPassDescription& desc);
		void DestroyRenderPass(const RenderPassDescription& desc);
		VkRenderPass GetRenderPass(const RenderPassDescription& desc) const;

		PipelineVk* CreateGraphicsPipeline(const GraphicsPipelineDesc& desc, VkRenderPass renderPass);
		void DestroyGraphicsPipeline(const GraphicsPipelineDesc& desc);
		PipelineVk* GetGraphicsPipeline(const GraphicsPipelineDesc& desc);

		PipelineVk* CreateComputePipeline(const ComputePipelineDesc& desc);
		void DestroyComputePipeline(const ComputePipelineDesc& desc);
		PipelineVk* GetComputePipeline(const ComputePipelineDesc& desc);

		// Getters
		VkDevice GetLogicalDevice() const;
		VkPhysicalDevice GetPhysicalDevice() const;
		VmaAllocator GetAllocator() const;
		VkDescriptorPool GetDescriptorPool() const;
		VkCommandPool GetCommandPool(QUEUE_TYPE type) const;
		VkQueue GetQueue(QUEUE_TYPE type) const;

	private:

		STATUS_CODE CreateVMAAllocator();

		STATUS_CODE CreatePhysicalDevice(VkSurfaceKHR surface);
		STATUS_CODE CreateLogicalDevice(VkSurfaceKHR surface);

		STATUS_CODE AllocateDescriptorPool();

		STATUS_CODE AllocateCommandPools();
		STATUS_CODE AllocateCommandPool_Helper(QUEUE_TYPE type, VkCommandPoolCreateFlags flags);

	private:

		VmaAllocator m_allocator;

		VkDevice m_logicalDevice;
		VkPhysicalDevice m_physicalDevice;
		std::unordered_map<QUEUE_TYPE, VkQueue> m_queues;
		QueueFamilyIndices m_queueFamilyIndices;

		// Physical device cache
		VkPhysicalDeviceProperties m_physicalDeviceProperties;
		VkPhysicalDeviceFeatures m_physicalDeviceFeatures;
		VkPhysicalDeviceMemoryProperties m_physicalDeviceMemoryProperties;

		// Descriptor pool
		VkDescriptorPool m_descriptorPool;

		// Command pools
		std::unordered_map<QUEUE_TYPE, VkCommandPool> m_commandPools;

		// Object caches
		FramebufferCache m_framebufferCache;
		RenderPassCache m_renderPassCache;
		PipelineCache* m_pipelineCache;
	};
}