
#include <vulkan/vk_enum_string_helper.h>

#include "device_context_vk.h"

#include "buffer_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "swap_chain_vk.h"
#include "uniform_vk.h"
#include "utils/logger.h"
#include "utils/sanity.h"

#define VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, msg) if(cmdBuffer == VK_NULL_HANDLE) { LogError(msg); return STATUS_CODE::ERR_INTERNAL; }

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

	STATUS_CODE DeviceContextVk::BeginFrame(ISwapChain* pSwapChain)
	{
		if (m_pRenderDevice == VK_NULL_HANDLE)
		{
			LogError("Failed to begin frame! Render device is null");
			return STATUS_CODE::ERR_INTERNAL;
		}

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(pSwapChain);
		if (swapChainVk == nullptr)
		{
			LogError("Failed to begin frame! Swap chain pointer is null");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res;
		res = swapChainVk->AcquireNextImage();
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Swap chain could not acquire next image");
			return res;
		}

		for (auto& cmdBuffer : m_cmdBuffers)
		{
			vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		}
		m_cmdBuffers.clear();

		// Create the primary command buffer that will run all secondary commands
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		res = CreateCommandBuffer(QUEUE_TYPE::GRAPHICS, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Primary command buffer creation failed");
			return res;
		}
		m_cmdBuffers.push_back(cmdBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Is this right?
		beginInfo.pInheritanceInfo = nullptr;
		VkResult resVk = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to begin frame. Primary command buffer failed to start recording. Got error: %s", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Flush()
	{
		VkCommandBuffer primaryCmdBuffer = GetPrimaryCommandBuffer();
		VkSubmitInfo vkSubmitInfo{};
		vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		vkSubmitInfo.waitSemaphoreCount = 0; // TODO
		vkSubmitInfo.pWaitSemaphores = nullptr; // TODO
		vkSubmitInfo.pWaitDstStageMask = nullptr; // TODO
		vkSubmitInfo.commandBufferCount = 1;
		vkSubmitInfo.pCommandBuffers = &primaryCmdBuffer;
		vkSubmitInfo.signalSemaphoreCount = 0; // TODO
		vkSubmitInfo.pSignalSemaphores = nullptr; // TODO

		VkQueue graphicsQueue = m_pRenderDevice->GetQueue(QUEUE_TYPE::GRAPHICS);
		VkResult res = vkQueueSubmit(graphicsQueue, 1, &vkSubmitInfo, VK_NULL_HANDLE);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to flush command buffers! Submit call failed with error: %s", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		res = vkQueueWaitIdle(graphicsQueue);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to wait until queue became idle after submitting! Got error: %s", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BeginRenderPass(IFramebuffer* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount)
	{
		// Framebuffer
		FramebufferVk* framebufferVk = static_cast<FramebufferVk*>(pFramebuffer);
		if (framebufferVk == nullptr)
		{
			LogError("Failed to begin render pass! Framebuffer is null");
			return STATUS_CODE::ERR_API;
		}

		// Render pass from framebuffer
		const auto& renderPassDesc = framebufferVk->GetRenderPassDescription();
		VkRenderPass renderPass = RenderPassCache::Get().Find(renderPassDesc);
		if (renderPass == VK_NULL_HANDLE)
		{
			LogError("Failed to begin render pass! Render pass bound to framebuffer does not exist");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Process clear values
		std::vector<VkClearValue> vkClearValues(clearColorCount);
		for (u32 i = 0; i < clearColorCount; i++)
		{
			VkClearValue clearValues{};
			if (pClearColors[i].isClearColor)
			{
				memcpy(&clearValues.color.float32, &pClearColors[i].color.color, sizeof(float) * 4);
			}
			else
			{
				clearValues.depthStencil.depth = pClearColors[i].depthStencil.depthClear;
				clearValues.depthStencil.stencil = pClearColors[i].depthStencil.stencilClear;
			}

			vkClearValues[i] = clearValues;
		}

		// Begin a new render pass on the primary command buffer
		VkCommandBuffer primaryCmdBuffer = GetPrimaryCommandBuffer();

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebufferVk->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { framebufferVk->GetWidth(), framebufferVk->GetHeight() };
		renderPassInfo.clearValueCount = clearColorCount;
		renderPassInfo.pClearValues = (pClearColors == nullptr) ? nullptr : vkClearValues.data();
		vkCmdBeginRenderPass(primaryCmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		// Create a new secondary command buffer to record draw call
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::GRAPHICS, false, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to create new command buffer to start render pass!");
			return res;
		}
		m_cmdBuffers.push_back(cmdBuffer);

		VkCommandBufferInheritanceInfo inheritanceInfo{};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.pNext = nullptr;
		inheritanceInfo.renderPass = renderPass;
		inheritanceInfo.subpass = 0;
		inheritanceInfo.framebuffer = framebufferVk->GetFramebuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Is this correct?
		beginInfo.pInheritanceInfo = &inheritanceInfo;
		VkResult resVk = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to start recording command buffer! Got error code: %s", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndRenderPass()
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			LogError("Failed to end render pass! No command buffers exist");
			return STATUS_CODE::ERR_INTERNAL;
		}
		vkEndCommandBuffer(cmdBuffer);

		VkCommandBuffer primaryCmdBuffer = GetPrimaryCommandBuffer();
		u32 cmdBufferCount = static_cast<u32>(m_cmdBuffers.size() - 1); // Skip the first command buffer, which is the primary one
		vkCmdExecuteCommands(primaryCmdBuffer, cmdBufferCount, m_cmdBuffers.data() + 1);
		vkCmdEndRenderPass(primaryCmdBuffer);
		vkEndCommandBuffer(primaryCmdBuffer);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindVertexBuffer(IBuffer* pVertexBuffer)
	{
		BufferVk* vBufferVk = static_cast<BufferVk*>(pVertexBuffer);
		if (vBufferVk == nullptr)
		{
			LogError("Failed to bind vertex buffer! Vertex buffer is null");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == nullptr)
		{
			LogError("Failed to bind vertex buffer! No command buffers exist");
			return STATUS_CODE::ERR_INTERNAL;
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
			return STATUS_CODE::ERR_API;
		}

		BufferVk* iBufferVk = static_cast<BufferVk*>(pIndexBuffer);
		if (iBufferVk == nullptr)
		{
			LogError("Failed to bind mesh! Index buffer is null");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer();
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			LogError("Failed to bind mesh! No command buffers exist");
			return STATUS_CODE::ERR_INTERNAL;
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
			return STATUS_CODE::ERR_API;
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
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		res = CreateCommandBuffer(QUEUE_TYPE::TRANSFER, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to buffer! Command buffer creation failed");
			return res;
		}

		res = bufferVk->CopyData(data, sizeBytes);
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
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

		LogWarning("Waiting for transfer queue to be idle when copying data to buffer");
		vkQueueWaitIdle(transferQueue);

		return STATUS_CODE::SUCCESS;
	}

	//STATUS_CODE DeviceContextVk::TEMP_TransitionTextureToGeneralLayout(ITexture* pTexture)
	//{
	//	return TEMP_TransitionTexture(pTexture, VK_IMAGE_LAYOUT_GENERAL);
	//}

	//STATUS_CODE DeviceContextVk::TEMP_TransitionTextureToPresentLayout(ITexture* pTexture)
	//{
	//	return TEMP_TransitionTexture(pTexture, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	//}

	STATUS_CODE DeviceContextVk::CreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer, VkCommandBuffer& out_cmdBuffer)
	{
		if (m_pRenderDevice == nullptr)
		{
			LogError("Failed to create command buffer. Render device is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = isPrimaryCmdBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		allocInfo.commandPool = m_pRenderDevice->GetCommandPool(type);
		allocInfo.commandBufferCount = 1;

		VkResult res = vkAllocateCommandBuffers(m_pRenderDevice->GetLogicalDevice(), &allocInfo, &out_cmdBuffer);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to allocate primary command buffer! Got result: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
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

	VkCommandBuffer DeviceContextVk::GetPrimaryCommandBuffer()
	{
		if (m_cmdBuffers.size() == 0)
		{
			return VK_NULL_HANDLE;
		}

		return m_cmdBuffers.at(0);
	}

	//STATUS_CODE DeviceContextVk::TEMP_TransitionTexture(ITexture* pTexture, VkImageLayout layout)
	//{
	//	TextureVk* textureVk = static_cast<TextureVk*>(pTexture);
	//	if (textureVk == nullptr)
	//	{
	//		LogError("TEMP - Failed to transition texture! Texture is null");
	//		return STATUS_CODE::ERR;
	//	}

	//	VkPipelineStageFlags sourceStage;
	//	VkPipelineStageFlags destinationStage;
	//	QUEUE_TYPE queueType;
	//	VkImageMemoryBarrier imageBarrier;

	//	// If false is returned, Current layout is the same as destination, nothing more to do
	//	if (textureVk->FillTransitionLayoutInfo(layout, sourceStage, destinationStage, queueType, imageBarrier))
	//	{
	//		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
	//		STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::TRANSFER, true, cmdBuffer);
	//		if (res != STATUS_CODE::SUCCESS)
	//		{
	//			LogError("TEMP - Failed to transition texture! Command buffer creation failed");
	//			return res;
	//		}

	//		VkCommandBufferBeginInfo beginInfo{};
	//		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	//		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	//		vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	//		vkCmdPipelineBarrier(
	//			cmdBuffer, 
	//			sourceStage, 
	//			destinationStage, 
	//			0,
	//			0, nullptr, // No memory barriers
	//			0, nullptr, // No buffer barriers
	//			1, &imageBarrier
	//		);

	//		vkEndCommandBuffer(cmdBuffer);

	//		VkSubmitInfo submitInfo{};
	//		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	//		submitInfo.commandBufferCount = 1;
	//		submitInfo.pCommandBuffers = &cmdBuffer;

	//		VkQueue transferQueue = m_pRenderDevice->GetQueue(QUEUE_TYPE::TRANSFER);
	//		if (transferQueue == VK_NULL_HANDLE)
	//		{
	//			LogError("TEMP - Failed to transition texture! Transfer queue does not exist");
	//			vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	//			return STATUS_CODE::ERR;
	//		}

	//		vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

	//		LogWarning("Waiting for transfer queue to be idle when copying data to buffer");
	//		vkQueueWaitIdle(transferQueue);

	//		textureVk->SetLayout(layout);
	//	}

	//	return STATUS_CODE::SUCCESS;
	//}
}