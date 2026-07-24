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

	// A single, contiguous run of commands recorded for one queue. Consecutive passes that
	// use the same queue share a batch's command buffer; a queue switch (e.g. graphics -> compute)
	// starts a new batch. Batches are submitted in recording (render-graph dependency) order and
	// chained together with binary semaphores so that cross-queue producer/consumer dependencies
	// are respected on the GPU.
	struct SubmissionBatch
	{
		QUEUE_TYPE queueType       = QUEUE_TYPE::GRAPHICS;
		u32 queueFamilyIndex       = QueueFamilyIndices::INVALID_INDEX;
		VkCommandBuffer cmdBuffer  = VK_NULL_HANDLE;
	};

	class DeviceContextVk : public IDeviceContext
	{
	public:

		DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo);
		~DeviceContextVk();

		STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer) override;
		STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer, INDEX_TYPE indexType) override;
		STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection) override;
		STATUS_CODE FlushUniformUpdates(UniformCollectionHandle uniformCollection) override;
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

		void SetMetricsPointer(Metrics* pMetrics) override;
		void ResetMetricsPointer() override;

		// Configures the query pool for this frame's timestamp queries
		void SetQueryPool(VkQueryPool queryPool, u32 frameBaseQueryIndex);
		void ResetQueryPool();

		// Writes the end-of-frame timestamp into the last recorded command buffer
		STATUS_CODE WriteEndTimestamp();

		// This is called by the current render pass during baking, so that the device context
		// is aware of the pipeline contextually and can use it directly. This is different
		// from the previous approach that sent the client a pipeline object, which the
		// client had to pass back in
		STATUS_CODE SetContextualPipeline(PipelineVk* pPipeline);
		void ResetContextualPipeline();

		STATUS_CODE BeginFrame(SwapChainVk* pSwapChain);
		STATUS_CODE EndFrame();

		STATUS_CODE BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount);
		STATUS_CODE EndRenderPass();

		// Inserts a debug label (marker region) into the command buffer for the given queue type
		STATUS_CODE BeginLabel(QUEUE_TYPE queueType, const char* name);
		STATUS_CODE EndLabel(QUEUE_TYPE queueType);

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

		// Returns the command buffer from the current (most recent) batch if it targets
		// the same queue family. This is intra-frame reuse — consecutive commands on the
		// same queue family share a single command buffer and submission.
		bool TryReuseActiveCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer);

		// Acquires a command buffer for a new batch: pulls from the cross-frame cache
		// (already reset by vkResetCommandPool at frame start) or allocates a new one,
		// begins recording, and registers a new submission batch.
		STATUS_CODE AllocateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer);

		// Thin wrapper: tries intra-frame reuse first, then acquires a new command buffer.
		STATUS_CODE GetOrCreateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer);

		void DeallocateCommandBuffers();
		void ResetCommandBuffers();

		// Ensures at least 'count' chain semaphores exist for this frame slot, creating more as needed.
		// Chain semaphores are reused every frame (the BeginFrame fence wait guarantees they are unsignaled).
		STATUS_CODE EnsureChainSemaphores(u32 count);
		void DestroyChainSemaphores();

		// TODO - MOVE TO UTILS!
		// Returns the queue type from the bind point. May return invalid result in the form of QUEUE_TYPE::COUNT!
		QUEUE_TYPE GetQueueTypeFromBindPoint(VkPipelineBindPoint bindPoint);

		STATUS_CODE FlushInternal(QUEUE_TYPE queueType, const VkCommandBuffer* pCommandBuffers, u32 commandBufferCount, const FlushSyncData& syncData);

		StagingAllocation AllocateStaging(u64 sizeBytes, u64 alignment = 16);
		void ResetStagingPool();

	private:

		RenderDeviceVk* m_pRenderDevice;

		// Ordered list of submission batches recorded this frame, in render-graph dependency order.
		// Each batch owns a single command buffer bound to one queue.
		std::vector<SubmissionBatch> m_submissionBatches;

		// Per-queue-type cache of command buffers that persist across frames. ResetCommandBuffers
		// calls vkResetCommandPool (bulk-resetting all command buffers to initial state) then
		// returns used command buffers here. AcquireCommandBuffer pulls from here without needing
		// to call vkResetCommandBuffer individually, or allocates new ones if the cache is empty.
		std::array<std::vector<VkCommandBuffer>, static_cast<u32>(QUEUE_TYPE::COUNT)> m_commandBufferCache;

		// Binary semaphores used to chain consecutive submission batches together (batch i signals
		// m_chainSemaphores[i], batch i+1 waits on it). Grown on demand and reused across frames.
		std::vector<VkSemaphore> m_chainSemaphores;

		// Staging buffer pool for efficient sub-allocation. Avoids creating thousands
		// of individual VMA allocations when uploading many textures/mip levels
		StagingBufferPool m_stagingPool;

		// True if any command buffers were submitted last frame (tells BeginFrame whether the
		// frame fence will actually be signaled, so it knows whether to wait on it).
		bool m_workSubmitted;

		// Assigned frame index, unique per device context
		u32 m_assignedFrameIndex;

		// Non-owning
		PipelineVk* m_contextualPipeline;

		// Non-owning, nullable
		Metrics* m_pMetrics;

		// Non-owning. Set by RenderGraphVk before Bake() to enable GPU timestamp queries.
		// When non-null, the first graphics/compute command buffer created gets a begin
		// timestamp, and WriteEndTimestamp() writes the end timestamp into the last command buffer.
		VkQueryPool m_queryPool;
		u32 m_queryFrameBaseIndex;
		bool m_beginTimestampWritten;
	};
}