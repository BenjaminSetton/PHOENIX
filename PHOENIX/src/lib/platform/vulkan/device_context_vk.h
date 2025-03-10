#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/device_context.h"
#include "render_device_vk.h"

namespace PHX
{
	class DeviceContextVk : public IDeviceContext
	{
	public:

		DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo);
		~DeviceContextVk();

		STATUS_CODE BeginFrame() override;
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

	private:

		STATUS_CODE CreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer);
		VkCommandBuffer* GetLastCommandBuffer();

	private:

		RenderDeviceVk* m_pRenderDevice; // Not sure if this is a good idea or not :/

		std::vector<VkCommandBuffer> m_cmdBuffers;
	};
}