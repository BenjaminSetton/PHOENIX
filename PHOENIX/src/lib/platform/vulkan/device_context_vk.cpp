
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
		UNUSED(createInfo);

		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create a device context, but the render device is null!");
			return;
		}
		m_pRenderDevice = pRenderDevice;

		// Create the primary command buffer that will run all secondary commands
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::GRAPHICS, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to create device context. Primary command buffer creation failed!");
			return;
		}
		m_primaryCmdBuffer = cmdBuffer;
	}

	DeviceContextVk::~DeviceContextVk()
	{
		FreeCachedCommandBuffers();
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

		vkQueueWaitIdle(transferQueue);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BeginFrame(SwapChainVk* pSwapChain, u32 frameIndex)
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

		// Only block on the in-flight fence if work was submitted for the corresponding frame
		VkFence inFlightFence = m_pRenderDevice->GetInFlightFence(frameIndex);
		if (m_wasWorkSubmitted)
		{
			// Wait until rendering is done for the last frame-in-flight with the same index
			vkWaitForFences(m_pRenderDevice->GetLogicalDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);
		}

		VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(frameIndex);
		res = swapChainVk->AcquireNextImage(imageAvailableSemaphore);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Swap chain could not acquire next image");
			return res;
		}

		FreeSecondaryCommandBuffers();
		m_cmdBuffers.clear();

		vkResetCommandBuffer(m_primaryCmdBuffer, 0);

		// Only reset the fence if we're submitting work, otherwise we might deadlock
		if (m_wasWorkSubmitted)
		{
			vkResetFences(m_pRenderDevice->GetLogicalDevice(), 1, &inFlightFence);
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Is this right?
		beginInfo.pInheritanceInfo = nullptr;
		VkResult resVk = vkBeginCommandBuffer(m_primaryCmdBuffer, &beginInfo);
		if (resVk != VK_SUCCESS)
		{
			LogError("Failed to begin frame. Primary command buffer failed to start recording. Got error: %s", string_VkResult(resVk));
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_wasWorkSubmitted = false;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Flush(SwapChainVk* pSwapChain, u32 frameIndex)
	{
		if (pSwapChain == nullptr)
		{
			LogError("Failed to flush device context. Swap chain is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (m_cmdBuffers.size() <= 0)
		{
			LogWarning("Attempting to flush a frame but no command buffers have been submitted!");
			return STATUS_CODE::ERR_API;
		}

		m_wasWorkSubmitted = true;

		// End the primary command buffer
		vkCmdExecuteCommands(m_primaryCmdBuffer, static_cast<u32>(m_cmdBuffers.size()), m_cmdBuffers.data());
		vkCmdEndRenderPass(m_primaryCmdBuffer);
		vkEndCommandBuffer(m_primaryCmdBuffer);

		// Submit all commands
		VkFence inFlightFence = m_pRenderDevice->GetInFlightFence(frameIndex);
		VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(frameIndex);
		VkPipelineStageFlags waitDstFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		VkSubmitInfo vkSubmitInfo{};
		vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		vkSubmitInfo.waitSemaphoreCount = 1;
		vkSubmitInfo.pWaitSemaphores = &imageAvailableSemaphore;
		vkSubmitInfo.pWaitDstStageMask = &waitDstFlags;
		vkSubmitInfo.commandBufferCount = 1;
		vkSubmitInfo.pCommandBuffers = &m_primaryCmdBuffer;
		vkSubmitInfo.signalSemaphoreCount = 0; // TODO
		vkSubmitInfo.pSignalSemaphores = nullptr; // TODO

		VkQueue graphicsQueue = m_pRenderDevice->GetQueue(QUEUE_TYPE::GRAPHICS);
		VkResult res = vkQueueSubmit(graphicsQueue, 1, &vkSubmitInfo, inFlightFence);
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

	STATUS_CODE DeviceContextVk::BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount)
	{
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
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = pFramebuffer->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { pFramebuffer->GetWidth(), pFramebuffer->GetHeight() };
		renderPassInfo.clearValueCount = clearColorCount;
		renderPassInfo.pClearValues = (pClearColors == nullptr) ? nullptr : vkClearValues.data();
		vkCmdBeginRenderPass(m_primaryCmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

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
		inheritanceInfo.framebuffer = pFramebuffer->GetFramebuffer();

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

		return STATUS_CODE::SUCCESS;
	}

	// TODO - Perform layout transition without creating a new command buffer in transfer queue and blocking.
	//        Also, have the transition details exposed as function parameters rather than assuming src/dst stages and access masks
	STATUS_CODE DeviceContextVk::TransitionImageLayout(ITexture* pTexture, VkImageLayout destinationLayout)
	{
		TextureVk* textureVk = static_cast<TextureVk*>(pTexture);
		if (textureVk == nullptr)
		{
			LogError("Failed to transition texture! Texture is null");
			return STATUS_CODE::ERR_API;
		}

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		QUEUE_TYPE queueType;
		VkImageMemoryBarrier imageBarrier;

		// If false is returned, Current layout is the same as destination, nothing more to do
		if (textureVk->FillTransitionLayoutInfo(destinationLayout, sourceStage, destinationStage, queueType, imageBarrier))
		{
			VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
			STATUS_CODE res = CreateCommandBuffer(QUEUE_TYPE::TRANSFER, true, cmdBuffer);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to transition texture! Command buffer creation failed");
				return res;
			}

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(cmdBuffer, &beginInfo);

			vkCmdPipelineBarrier(
				cmdBuffer,
				sourceStage,
				destinationStage,
				0,
				0, nullptr, // No memory barriers
				0, nullptr, // No buffer barriers
				1, &imageBarrier
			);

			vkEndCommandBuffer(cmdBuffer);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuffer;

			VkQueue transferQueue = m_pRenderDevice->GetQueue(QUEUE_TYPE::TRANSFER);
			if (transferQueue == VK_NULL_HANDLE)
			{
				LogError("Failed to transition texture! Transfer queue does not exist");
				vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				return STATUS_CODE::ERR_INTERNAL;
			}

			vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);

			LogWarning("Waiting for transfer queue to be idle when copying data to buffer");
			vkQueueWaitIdle(transferQueue);

			textureVk->SetLayout(destinationLayout);
		}

		return STATUS_CODE::SUCCESS;
	}

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

	void DeviceContextVk::FreeCommandBuffer(VkCommandBuffer cmdBuffer)
	{
		// TODO - Delete commands from the correct pool
		VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(QUEUE_TYPE::GRAPHICS);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, 1, &cmdBuffer);
	}

	void DeviceContextVk::FreeCachedCommandBuffers()
	{
		// TODO - Delete commands from the correct pool

		// Free secondary command buffers
		VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(QUEUE_TYPE::GRAPHICS);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, static_cast<u32>(m_cmdBuffers.size()), m_cmdBuffers.data());

		// Free primary command buffer
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, 1, &m_primaryCmdBuffer);

		m_primaryCmdBuffer = VK_NULL_HANDLE;
		m_cmdBuffers.clear();
	}

	void DeviceContextVk::FreeSecondaryCommandBuffers()
	{
		if (m_cmdBuffers.size() <= 0)
		{
			// Nothing to free
			return;
		}

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