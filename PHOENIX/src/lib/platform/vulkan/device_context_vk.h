#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/device_context.h"
#include "render_device_vk.h"
#include "texture_vk.h"

namespace PHX
{
	class DeviceContextVk : public IDeviceContext
	{
	public:

		DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo);
		~DeviceContextVk();

		STATUS_CODE BeginFrame(ISwapChain* pSwapChain) override;
		STATUS_CODE Flush() override;

		STATUS_CODE BeginRenderPass(IFramebuffer* pFramebuffer) override;
		STATUS_CODE EndRenderPass() override;

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

		// NOTE - We need more abstraction so that the client doesn't have to manually change the 
		// texture layouts because older APIs don't have a concept of that. I think this would belong 
		// in a render-graph system but a temporary call will do for now
		//STATUS_CODE TEMP_TransitionTextureToGeneralLayout(ITexture* pTexture);
		//STATUS_CODE TEMP_TransitionTextureToPresentLayout(ITexture* pTexture);

	private:

		STATUS_CODE CreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer, VkCommandBuffer& out_cmdBuffer);
		void DestroyCommandBuffer(VkCommandBuffer cmdBuffer);
		void DestroyCachedCommandBuffers();

		VkCommandBuffer GetLastCommandBuffer();
		VkCommandBuffer GetPrimaryCommandBuffer();

		// NOTE - We need more abstraction so that the client doesn't have to manually change the 
		// texture layouts because older APIs don't have a concept of that. I think this would belong 
		// in a render-graph system but a temporary call will do for now
		//STATUS_CODE TEMP_TransitionTexture(ITexture* pTexture, VkImageLayout layout);

	private:

		RenderDeviceVk* m_pRenderDevice; // Not sure if this is a good idea or not :/

		// Do we want to guarantee that all of these command buffers were allocated from the same pool?
		std::vector<VkCommandBuffer> m_cmdBuffers;
	};
}