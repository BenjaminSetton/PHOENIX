#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "buffer_vk.h"
#include "PHX/interface/device_context.h"
#include "render_device_vk.h"
#include "texture_vk.h"

namespace PHX
{
	// Forward declarations
	class StagingBufferVk;
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

		STATUS_CODE BindVertexBuffer(IBuffer* pVertexBuffer) override;
		STATUS_CODE BindMesh(IBuffer* pVertexBuffer, IBuffer* pIndexBuffer) override;
		STATUS_CODE BindUniformCollection(IUniformCollection* pUniformCollection, IPipeline* pPipeline) override;
		STATUS_CODE BindPipeline(IPipeline* pPipeline) override;
		STATUS_CODE SetViewport(Vec2u size, Vec2u offset) override;
		STATUS_CODE SetScissor(Vec2u size, Vec2u offset) override;

		STATUS_CODE Draw(u32 vertexCount) override;
		STATUS_CODE DrawIndexed(u32 indexCount) override;
		STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount) override;

		STATUS_CODE Dispatch(Vec3u dimensions) override;

		STATUS_CODE CopyDataToBuffer(IBuffer* pBuffer, const void* data, u64 sizeBytes) override;
		STATUS_CODE CopyDataToTexture(ITexture* pTexture, const void* data, u64 sizeBytes) override;

		STATUS_CODE BeginFrame(SwapChainVk* pSwapChain, u32 frameIndex);
		STATUS_CODE EndFrame(u32 frameIndex);

		STATUS_CODE Flush(QUEUE_TYPE queueType, const FlushSyncData& syncData);

		STATUS_CODE BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount);
		STATUS_CODE EndRenderPass();

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

	private:

		STATUS_CODE GetOrCreateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer);
		void DeallocateCommandBuffers();

		// Returns the queue type from the pipeline bind point. May return invalid result in the form of QUEUE_TYPE::COUNT!
		QUEUE_TYPE GetQueueTypeFromPipelineBindPoint(VkPipelineBindPoint vkBindPoint);

		STATUS_CODE FlushInternal(QUEUE_TYPE queueType, const VkCommandBuffer* pCommandBuffers, u32 commandBufferCount, const FlushSyncData& syncData);

		StagingBufferVk* CreateStagingBuffer(const BufferCreateInfo& createInfo);
		void DestroyStagingBuffers();

	private:

		RenderDeviceVk* m_pRenderDevice;

		// Stores all command buffers from all supported queues
		std::array<CommandBufferList, static_cast<size_t>(QUEUE_TYPE::COUNT)> m_cmdBuffers;

		// Cache the staging buffers used in the current frame. Since staging buffer are RAII objects, they must
		// live outside the scope of the functions that create them at least until the commands that reference
		// the buffers are executed
		std::vector<StagingBufferVk*> m_stagingBuffers;

		bool m_wasWorkSubmitted;
	};
}