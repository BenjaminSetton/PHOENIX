#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "buffer_vk.h"
#include "core/interface_types/device_context_interface.h"
#include "render_device_vk.h"
#include "texture_vk.h"
#include "utils/staging_buffer_pool.h"

namespace PHX
{
	// Forward declarations
	class SwapChainVk;

	using CommandBufferList = std::vector<VkCommandBuffer>;

	struct FlushSyncData
	{
		VkSemaphore* pWaitSemaphores   = nullptr;
		u32 waitSemaphoreCount         = 0;
		VkSemaphore* pSignalSemaphores = nullptr;
		u32 signalSemaphoreCount       = 0;
		VkFence signalFence            = VK_NULL_HANDLE;
	};

	class DeviceContextVk : public IDeviceContext
	{
	public:

		DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo);
		~DeviceContextVk();

		STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer) override;
		STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer, INDEX_TYPE indexType) override;
		STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection) override;
		STATUS_CODE SetViewport(Vec2u size, Vec2u offset) override;
		STATUS_CODE SetScissor(Vec2u size, Vec2u offset) override;

		STATUS_CODE Draw(u32 vertexCount) override;
		STATUS_CODE DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset) override;
		STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 instanceOffset) override;

		STATUS_CODE Dispatch(Vec3u dimensions) override;
		STATUS_CODE TraceRays(Vec3u dimensions) override;

		STATUS_CODE BuildBottomLevelAccelerationStructure(AccelerationStructureHandle handle) override;
		STATUS_CODE BuildTopLevelAccelerationStructure(AccelerationStructureHandle handle, BufferHandle instanceBuffer, u32 instanceCount) override;

		STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes) override;
		STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes, u32 mipLevel) override;

		// This is called by the current render pass during baking, so that the device context
		// is aware of the pipeline contextually and can use it directly. This is different
		// from the previous approach that sent the client a pipeline object, which the
		// client had to pass back in
		STATUS_CODE SetContextualPipeline(PipelineVk* pPipeline);
		void ResetContextualPipeline();

		STATUS_CODE BeginFrame(SwapChainVk* pSwapChain, u32 frameIndex);
		STATUS_CODE EndFrame(u32 frameIndex);

		STATUS_CODE BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount);
		STATUS_CODE EndRenderPass();

		STATUS_CODE Flush(QUEUE_TYPE queueType, const FlushSyncData& syncData);

		// TODO - Have the transition details exposed as function parameters rather than assuming src/dst stages and access masks
		//STATUS_CODE TransitionImageLayout(TextureVk* pTexture, VkImageLayout destinationLayout, VkCommandBuffer cmdBuffer = VK_NULL_HANDLE);

		STATUS_CODE InsertImageMemoryBarrier(
			TextureVk* pTexture,
			QUEUE_TYPE queueType,
			VkPipelineStageFlags srcStageMask, 
			VkPipelineStageFlags dstStageMask, 
			VkAccessFlags srcAccessMask, 
			VkAccessFlags dstAccessMask,
			VkImageLayout oldLayout,
			VkImageLayout newLayout
		);

		STATUS_CODE InsertBufferMemoryBarrier(
			BufferVk* pBuffer,
			QUEUE_TYPE queueType,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask
		);

		STATUS_CODE InsertAccelerationStructureMemoryBarrier(
			AccelerationStructureVk* pAS,
			QUEUE_TYPE queueType,
			VkPipelineStageFlags srcStageMask,
			VkPipelineStageFlags dstStageMask,
			VkAccessFlags srcAccessMask,
			VkAccessFlags dstAccessMask
		);

	private:

		STATUS_CODE GetOrCreateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer);
		void DeallocateCommandBuffers();


		// TODO - MOVE TO UTILS!
		// Returns the queue type from the bind point. May return invalid result in the form of QUEUE_TYPE::COUNT!
		QUEUE_TYPE GetQueueTypeFromBindPoint(VkPipelineBindPoint bindPoint);

		STATUS_CODE FlushInternal(QUEUE_TYPE queueType, const VkCommandBuffer* pCommandBuffers, u32 commandBufferCount, const FlushSyncData& syncData);

		StagingAllocation AllocateStaging(u64 sizeBytes, u64 alignment = 16);
		void ResetStagingPool();

	private:

		RenderDeviceVk* m_pRenderDevice;

		// Stores all command buffers from all supported queues
		std::array<CommandBufferList, static_cast<size_t>(QUEUE_TYPE::COUNT)> m_cmdBuffers;

		// Staging buffer pool for efficient sub-allocation. Avoids creating thousands
		// of individual VMA allocations when uploading many textures/mip levels
		StagingBufferPool m_stagingPool;

		bool m_wasWorkSubmitted;

		// Non-owning
		PipelineVk* m_contextualPipeline;
	};
}