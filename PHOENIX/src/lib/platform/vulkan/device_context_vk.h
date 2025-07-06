#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/device_context.h"
#include "render_device_vk.h"
#include "texture_vk.h"

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
		STATUS_CODE TransitionImageLayout(TextureVk* pTexture, VkImageLayout destinationLayout);

	private:

		STATUS_CODE CreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer, VkCommandBuffer& out_cmdBuffer);
		void FreeCommandBuffer(VkCommandBuffer cmdBuffer, QUEUE_TYPE queueType);

		void FreeCachedCommandBuffers();
		void FreeSecondaryCommandBuffers();

		VkCommandBuffer GetLastCommandBuffer(QUEUE_TYPE queueType);

		// Returns the queue type from the pipeline bind point. May return invalid result in the form of QUEUE_TYPE::COUNT!
		QUEUE_TYPE GetQueueTypeFromPipelineBindPoint(VkPipelineBindPoint vkBindPoint);

	private:

		RenderDeviceVk* m_pRenderDevice;

		// Primary graphics command buffer, allocated from graphics command pool
		VkCommandBuffer m_primaryCmdBuffer;

		// Stores all command buffers from all supported queues
		std::array<CommandBufferList, static_cast<size_t>(QUEUE_TYPE::COUNT)> m_cmdBuffers;

		bool m_wasWorkSubmitted;
	};
}