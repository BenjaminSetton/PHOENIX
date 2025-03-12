
#include <vulkan/vk_enum_string_helper.h>

#include "device_context_vk.h"

#include "buffer_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "uniform_vk.h"
#include "utils/logger.h"
#include "utils/sanity.h"

#define VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, msg) if(cmdBuffer == VK_NULL_HANDLE) { LogError(msg); return STATUS_CODE::ERR; }

namespace PHX
{

	DeviceContextVk::DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create a device context, but the render device is null!");
			return;
		}

		UNUSED(createInfo);
		m_pRenderDevice = pRenderDevice;
	}

	DeviceContextVk::~DeviceContextVk()
	{
		TODO();
	}

	STATUS_CODE DeviceContextVk::BeginFrame()
	{
		for (auto& cmdBuffer : m_cmdBuffers)
		{
			vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		}
		m_cmdBuffers.clear();

		// Create the primary command buffer that will run all secondary commands
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::GRAPHICS, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Primary command buffer creation failed");
			return STATUS_CODE::ERR;
		}

		m_cmdBuffers.push_back(cmdBuffer);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Flush()
	{
		TODO();
		return STATUS_CODE::ERR;
	}

	STATUS_CODE DeviceContextVk::BeginRenderPass(IFramebuffer* pFramebuffer)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::GRAPHICS, false, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to create new command buffer to start render pass!");
			return STATUS_CODE::ERR;
		}
		m_cmdBuffers.push_back(cmdBuffer);

		// Framebuffer
		FramebufferVk* framebufferVk = static_cast<FramebufferVk*>(pFramebuffer);
		ASSERT_PTR(framebufferVk);

		// Render pass from framebuffer
		const auto& renderPassDesc = framebufferVk->GetRenderPassDescription();
		VkRenderPass renderPass = RenderPassCache::Get().Find(renderPassDesc);
		if (renderPass == VK_NULL_HANDLE)
		{
			LogError("Failed to begin render pass! Render pass bound to framebuffer does not exist!");
			return STATUS_CODE::ERR;
		}

		VkCommandBufferInheritanceInfo inheritanceInfo{};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.pNext = nullptr;
		inheritanceInfo.renderPass = renderPass;
		inheritanceInfo.subpass = 0;
		inheritanceInfo.framebuffer = framebufferVk->GetFramebuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // TODO - Add flags
		beginInfo.pInheritanceInfo = &inheritanceInfo;
		VkResult resVk = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to start recording command buffer! Got error code: %s", string_VkResult(resVk));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndRenderPass()
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == nullptr)
		{
			LogError("Failed to end render pass! No command buffers exist");
			return STATUS_CODE::ERR;
		}

		vkEndCommandBuffer(cmdBuffer);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindVertexBuffer(IBuffer* pVertexBuffer)
	{
		BufferVk* vBufferVk = static_cast<BufferVk*>(pVertexBuffer);
		if (vBufferVk == nullptr)
		{
			LogError("Failed to bind vertex buffer! Vertex buffer is null");
			return STATUS_CODE::ERR;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == nullptr)
		{
			LogError("Failed to bind vertex buffer! No command buffers exist");
			return STATUS_CODE::ERR;
		}

		VkBuffer vertexBuffer = vBufferVk->GetBuffer();
		VkDeviceSize offset = vBufferVk->GetOffset();

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindMesh(IBuffer* pVertexBuffer, IBuffer* pIndexBuffer)
	{
		BufferVk* vBufferVk = static_cast<BufferVk*>(pVertexBuffer);
		if (vBufferVk == nullptr)
		{
			LogError("Failed to bind mesh! Vertex buffer is null");
			return STATUS_CODE::ERR;
		}

		BufferVk* iBufferVk = static_cast<BufferVk*>(pIndexBuffer);
		if (iBufferVk == nullptr)
		{
			LogError("Failed to bind mesh! Index buffer is null");
			return STATUS_CODE::ERR;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			LogError("Failed to bind mesh! No command buffers exist");
			return STATUS_CODE::ERR;
		}

		VkBuffer vertexBuffer = vBufferVk->GetBuffer();
		VkDeviceSize offset = vBufferVk->GetOffset();

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(cmdBuffer, iBufferVk->GetBuffer(), 0, VK_INDEX_TYPE_UINT32); // TODO - Support a range of sizes for index types?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindUniformCollection(IUniformCollection* pUniformCollection, IPipeline* pPipeline)
	{
		if (pUniformCollection == nullptr)
		{
			LogWarning("Attempting to bind uniform collection but the uniform collection is null!");
			return STATUS_CODE::SUCCESS;
		}

		if (pPipeline == nullptr)
		{
			// Pipeline object is required to bind uniform collection if the latter is not null
			LogError("Failed to bind uniform collection! Pipeline is null");
			return STATUS_CODE::ERR;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to bind uniform collection! No command buffers exist");

		PipelineVk* pipelineVk = static_cast<PipelineVk*>(pPipeline);
		ASSERT_PTR(pipelineVk);

		UniformCollectionVk* uniformCollectionVk = static_cast<UniformCollectionVk*>(pUniformCollection);
		ASSERT_PTR(uniformCollectionVk);

		vkCmdBindDescriptorSets(cmdBuffer, pipelineVk->GetBindPoint(), pipelineVk->GetLayout(), 0, uniformCollectionVk->GetDescriptorSetCount(), uniformCollectionVk->GetDescriptorSets(), 0, nullptr);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindPipeline(IPipeline* pPipeline)
	{
		if (pPipeline == nullptr)
		{
			LogWarning("Attempting to bind pipeline but the pipeline is null!");
			return STATUS_CODE::SUCCESS;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to bind uniform collection! No command buffers exist");

		PipelineVk* pipelineVk = static_cast<PipelineVk*>(pPipeline);
		ASSERT_PTR(pipelineVk);

		vkCmdBindPipeline(cmdBuffer, pipelineVk->GetBindPoint(), pipelineVk->GetPipeline());
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::SetViewport(Vec2u size, Vec2u offset)
	{
		if (size.GetX() == 0 && size.GetY() == 0)
		{
			LogWarning("Attempting to set viewport with a size of 0!");
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to set viewport! No command buffers exist");

		VkViewport viewport{};
		viewport.x = static_cast<float>(offset.GetX());
		viewport.y = static_cast<float>(offset.GetY());
		viewport.width = static_cast<float>(size.GetX());
		viewport.height = static_cast<float>(size.GetY());
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::SetScissor(Vec2u size, Vec2u offset)
	{
		if (size.GetX() == 0 && size.GetY() == 0)
		{
			LogWarning("Attempting to set scissor with a size of 0!");
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to set scissor! No command buffers exist");

		VkRect2D scissor{};
		scissor.offset = { static_cast<int>(offset.GetX()), static_cast<int>(offset.GetY()) };
		scissor.extent = { size.GetX(), size.GetY() };
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Draw(u32 vertexCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw call! No command buffers exist");

		vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexed(u32 indexCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw indexed call! No command buffers exist");

		vkCmdDrawIndexed(cmdBuffer, static_cast<u32>(indexCount), 1, 0, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexedInstanced(u32 indexCount, u32 instanceCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw indexed instanced call! No command buffers exist");

		vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, 0, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Dispatch(Vec3u dimensions)
	{
		// Properly support compute commands. These commands must be cleaned up from the correct pool
		TODO();

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue dispatch call! No command buffers exist");

		vkCmdDispatch(cmdBuffer, dimensions.GetX(), dimensions.GetY(), dimensions.GetZ());
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::CopyDataToBuffer(IBuffer* pBuffer, const void* data, u64 sizeBytes)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		BufferVk* bufferVk = static_cast<BufferVk*>(pBuffer);
		if (bufferVk == nullptr)
		{
			LogError("Failed to copy data to buffer! Buffer is null");
			return STATUS_CODE::ERR;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		res = CreateCommandBuffer(QUEUE_TYPE::TRANSFER, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to buffer! Command buffer creation failed");
			return res;
		}

		res = bufferVk->CopyDataToStagingBuffer(data, sizeBytes);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to buffer! Copy to staging buffer ran into an error");
			return res;
		}

		// Copy from staging buffer to GPU buffer
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdBuffer, &beginInfo);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = sizeBytes;
		vkCmdCopyBuffer(cmdBuffer, bufferVk->GetStagingBuffer(), bufferVk->GetBuffer(), 1, &copyRegion);

		vkEndCommandBuffer(cmdBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;

		VkQueue transferQueue = m_pRenderDevice->GetQueue(QUEUE_TYPE::TRANSFER);
		if (transferQueue == VK_NULL_HANDLE)
		{
			LogError("Failed to copy data to buffer! Transfer queue does not exist");
			vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
			return STATUS_CODE::ERR;
		}

		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(transferQueue);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::CreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer, VkCommandBuffer& out_cmdBuffer)
	{
		if (m_pRenderDevice == nullptr)
		{
			LogError("Failed to create command buffer. Render device is null!");
			return STATUS_CODE::ERR;
		}

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = isPrimaryCmdBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandPool = m_pRenderDevice->GetCommandPool(type);
		allocInfo.commandBufferCount = 1;

		VkResult res = vkAllocateCommandBuffers(m_pRenderDevice->GetLogicalDevice(), &allocInfo, &out_cmdBuffer);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to allocate primary command buffer! Got result: %s", string_VkResult(res));
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::DestroyCommandBuffer(VkCommandBuffer cmdBuffer)
	{
		// TODO - Delete commands from the correct pool
		VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(QUEUE_TYPE::GRAPHICS);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, 1, &cmdBuffer);
	}

	void DeviceContextVk::DestroyCachedCommandBuffers()
	{
		// TODO - Delete commands from the correct pool
		VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(QUEUE_TYPE::GRAPHICS);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, static_cast<u32>(m_cmdBuffers.size()), m_cmdBuffers.data());
	}

	VkCommandBuffer DeviceContextVk::GetLastCommandBuffer()
	{
		if (m_cmdBuffers.size() == 0)
		{
			return VK_NULL_HANDLE;
		}

		return m_cmdBuffers.at(m_cmdBuffers.size() - 1);
	}
}