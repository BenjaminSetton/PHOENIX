
#include <vulkan/vk_enum_string_helper.h>

#include "device_context_vk.h"

#include "buffer_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "swap_chain_vk.h"
#include "uniform_vk.h"
#include "utils/buffer_utils.h"
#include "utils/logger.h"
#include "utils/sanity.h"
#include "utils/staging_buffer.h"
#include "utils/texture_type_converter.h"

#define VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, msg) if(cmdBuffer == VK_NULL_HANDLE) { LogError(msg); return STATUS_CODE::ERR_INTERNAL; }

namespace PHX
{

	DeviceContextVk::DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo) :
		m_primaryCmdBuffer(VK_NULL_HANDLE), m_cmdBuffers(), m_wasWorkSubmitted(true)
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
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to create device context. Primary command buffer creation failed!");
			return;
		}
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

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
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

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
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

		PipelineVk* pipelineVk = static_cast<PipelineVk*>(pPipeline);
		ASSERT_PTR(pipelineVk);

		VkPipelineBindPoint vkBindPoint = pipelineVk->GetBindPoint();
		QUEUE_TYPE cmdQueueType = GetQueueTypeFromPipelineBindPoint(vkBindPoint);
		if (cmdQueueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to bind uniform collection. Pipeline has a VkPipelineBindPoint (%u) that's not graphics or compute!", static_cast<u32>(vkBindPoint));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(cmdQueueType);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to bind uniform collection! No command buffers exist");

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
			return STATUS_CODE::ERR_API;
		}

		PipelineVk* pipelineVk = static_cast<PipelineVk*>(pPipeline);
		ASSERT_PTR(pipelineVk);

		// Determine whether we're trying to bind a graphics or compute pipeline
		VkPipelineBindPoint vkBindPoint = pipelineVk->GetBindPoint();
		QUEUE_TYPE cmdQueueType = GetQueueTypeFromPipelineBindPoint(vkBindPoint);
		if (cmdQueueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to bind pipeline. Pipeline has a VkPipelineBindPoint (%u) that's not graphics or compute!", static_cast<u32>(vkBindPoint));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(cmdQueueType);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to bind uniform collection! No command buffers exist");

		vkCmdBindPipeline(cmdBuffer, vkBindPoint, pipelineVk->GetPipeline());
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::SetViewport(Vec2u size, Vec2u offset)
	{
		if (size.GetX() == 0 && size.GetY() == 0)
		{
			LogWarning("Attempting to set viewport with a size of 0!");
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
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

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to set scissor! No command buffers exist");

		VkRect2D scissor{};
		scissor.offset = { static_cast<int>(offset.GetX()), static_cast<int>(offset.GetY()) };
		scissor.extent = { size.GetX(), size.GetY() };
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Draw(u32 vertexCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw call! No command buffers exist");

		vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexed(u32 indexCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw indexed call! No command buffers exist");

		vkCmdDrawIndexed(cmdBuffer, static_cast<u32>(indexCount), 1, 0, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexedInstanced(u32 indexCount, u32 instanceCount)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue draw indexed instanced call! No command buffers exist");

		vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, 0, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Dispatch(Vec3u dimensions)
	{
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::COMPUTE);
		VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, "Failed to issue dispatch call! No command buffers exist");

		vkCmdDispatch(cmdBuffer, dimensions.GetX(), dimensions.GetY(), dimensions.GetZ());
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

		//FreeSecondaryCommandBuffers();

		//vkResetCommandBuffer(m_primaryCmdBuffer, 0);

		// Only reset the fence if we're submitting work, otherwise we might deadlock
		if (m_wasWorkSubmitted)
		{
			vkResetFences(m_pRenderDevice->GetLogicalDevice(), 1, &inFlightFence);
		}

		//VkCommandBufferBeginInfo beginInfo{};
		//beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // Is this right?
		//beginInfo.pInheritanceInfo = nullptr;
		//VkResult resVk = vkBeginCommandBuffer(m_primaryCmdBuffer, &beginInfo);
		//if (resVk != VK_SUCCESS)
		//{
		//	LogError("Failed to begin frame. Primary command buffer failed to start recording. Got error: %s", string_VkResult(resVk));
		//	return STATUS_CODE::ERR_INTERNAL;
		//}

		m_wasWorkSubmitted = false;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndFrame(u32 frameIndex)
	{
		CommandBufferList& cmdBufferList = m_cmdBuffers.at(static_cast<u32>(QUEUE_TYPE::GRAPHICS));
		if (cmdBufferList.size() <= 0)
		{
			LogError("Failed to end frame. No graphics work was submitted!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_wasWorkSubmitted = true;

		// End the primary command buffer
		vkCmdExecuteCommands(m_primaryCmdBuffer, static_cast<u32>(cmdBufferList.size()), cmdBufferList.data());
		vkCmdEndRenderPass(m_primaryCmdBuffer);
		vkEndCommandBuffer(m_primaryCmdBuffer);

		VkFence inFlightFence = m_pRenderDevice->GetInFlightFence(frameIndex);
		VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(frameIndex);

		FlushSyncData syncData{};
		syncData.pWaitSemaphores = &imageAvailableSemaphore;
		syncData.waitSemaphoreCount = 1;
		syncData.signalFence = inFlightFence;

		return Flush(QUEUE_TYPE::GRAPHICS, syncData);
	}

	STATUS_CODE DeviceContextVk::Flush(QUEUE_TYPE queueType, const FlushSyncData& syncData)
	{
		CommandBufferList& cmdBufferList = m_cmdBuffers.at(static_cast<u32>(queueType));
		if (cmdBufferList.size() <= 0)
		{
			// Nothing to flush
			return STATUS_CODE::SUCCESS;
		}

		VkCommandBuffer* buffersToSubmit = nullptr;
		u32 buffersToSubmitCount = 0;
		if (queueType == QUEUE_TYPE::GRAPHICS)
		{
			// Special case, only primary cmd buffer is submitted
			buffersToSubmit = &m_primaryCmdBuffer;
			buffersToSubmitCount = 1;
		}
		else
		{
			buffersToSubmit = cmdBufferList.data();
			buffersToSubmitCount = static_cast<u32>(cmdBufferList.size());
		}

		// End recording for all command buffers that will be submitted
		for (u32 i = 0; i < buffersToSubmitCount; i++)
		{
			VkCommandBuffer& cmdBuffer = buffersToSubmit[i];
			vkEndCommandBuffer(cmdBuffer);
		}

		// Submit all commands
		STATUS_CODE res = FlushInternal(queueType, buffersToSubmit, buffersToSubmitCount, syncData);

		// Free submitted command buffers now that we know they're not being used anymore
		if (queueType == QUEUE_TYPE::GRAPHICS)
		{
			vkResetCommandBuffer(m_primaryCmdBuffer, 0);
		}

		for (u32 i = 0; i < static_cast<u32>(cmdBufferList.size()); i++)
		{
			VkCommandBuffer cmdBuffer = cmdBufferList[i];
			FreeCommandBuffer(cmdBuffer, queueType);
		}
		cmdBufferList.clear();

		return res;
	}

	STATUS_CODE DeviceContextVk::BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount)
	{
		// Process clear values
		std::vector<VkClearValue> vkClearValues(clearColorCount);
		for (u32 i = 0; i < clearColorCount; i++)
		{
			VkClearValue clearValues{};
			if (pClearColors[i].useClearColor)
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
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, false, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to create new command buffer to start render pass!");
			return res;
		}

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
		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(QUEUE_TYPE::GRAPHICS);
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			LogError("Failed to end render pass! No command buffers exist");
			return STATUS_CODE::ERR_INTERNAL;
		}
		vkEndCommandBuffer(cmdBuffer);

		return STATUS_CODE::SUCCESS;
	}

	// TODO - Expose transition details as function parameters rather than assuming src/dst stages and access masks
	STATUS_CODE DeviceContextVk::TransitionImageLayout(TextureVk* pTexture, VkImageLayout destinationLayout, VkCommandBuffer cmdBuffer)
	{
		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;
		VkImageMemoryBarrier imageBarrier;

		const QUEUE_TYPE queue = QUEUE_TYPE::TRANSFER;

		// If false is returned, Current layout is the same as destination, nothing more to do
		if (pTexture->FillTransitionLayoutInfo(destinationLayout, sourceStage, destinationStage, imageBarrier))
		{
			bool createdOwnCommandBuffer = false;

			// Attempt to reuse existing transfer cmd buffer, if none is provided
			if (cmdBuffer == VK_NULL_HANDLE)
			{
				STATUS_CODE res = GetOrCreateCommandBuffer(queue, true, cmdBuffer);
				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to transition image layout. Could not retrive or create a valid command buffer!");
					return res;
				}

				createdOwnCommandBuffer = true;
			}

			if (createdOwnCommandBuffer)
			{
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(cmdBuffer, &beginInfo);
			}

			vkCmdPipelineBarrier(
				cmdBuffer,
				sourceStage,
				destinationStage,
				0,
				0, nullptr, // No memory barriers
				0, nullptr, // No buffer barriers
				1, &imageBarrier
			);

			if (createdOwnCommandBuffer)
			{
				vkEndCommandBuffer(cmdBuffer);
			}

			pTexture->SetLayout(destinationLayout);

			return STATUS_CODE::SUCCESS;
		}

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

		if (ShouldUseDirectMemoryMapping(bufferVk->GetUsage()))
		{
			// Copy to memory directly, no need to go through a staging buffer
			bufferVk->CopyToMappedData(data, sizeBytes);
		}
		else
		{
			// All other buffers must copy to staging buffer and then issue
			// a transfer command to copy the data over to the GPU
			BufferCreateInfo stagingBufferCI{}; // Buffer usage is unused in this case
			stagingBufferCI.sizeBytes = sizeBytes;
			StagingBufferVk stagingBuffer(m_pRenderDevice, stagingBufferCI);
			if (!stagingBuffer.IsValid())
			{
				LogError("Failed to copy data to buffer. Could not create staging buffer!");
				return STATUS_CODE::ERR_INTERNAL;
			}

			VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
			const QUEUE_TYPE transferQueueType = QUEUE_TYPE::TRANSFER;

			res = GetOrCreateCommandBuffer(transferQueueType, true, cmdBuffer);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to copy data to buffer! Command buffer creation failed");
				return res;
			}

			res = stagingBuffer.CopyData(data, sizeBytes);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to copy data to staging buffer!");
				return res;
			}

			// Copy from staging buffer to GPU buffer
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0; // Optional
			copyRegion.dstOffset = 0; // Optional
			copyRegion.size = sizeBytes;
			vkCmdCopyBuffer(cmdBuffer, stagingBuffer.GetBuffer(), bufferVk->GetBuffer(), 1, &copyRegion);
		}

		return res;
	}

	STATUS_CODE DeviceContextVk::CopyDataToTexture(ITexture* pTexture, const void* data, u64 sizeBytes)
	{
		if (pTexture == nullptr)
		{
			LogError("Failed to copy data to texture. Texture pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		if (data == nullptr)
		{
			LogError("Failed to copy data to texture. Data pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		if (sizeBytes <= 0)
		{
			LogError("Failed to copy data to texture. Size in bytes is 0!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res;
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		const QUEUE_TYPE transferQueueType = QUEUE_TYPE::TRANSFER;
		TextureVk* textureVk = static_cast<TextureVk*>(pTexture);
		ASSERT_PTR(textureVk);

		res = GetOrCreateCommandBuffer(transferQueueType, true, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to texture! Command buffer creation failed");
			return res;
		}

		// Create a staging buffer and copy data into staging buffer
		BufferCreateInfo stagingBufferCI{}; // Buffer usage is unused in this case
		stagingBufferCI.sizeBytes = sizeBytes;
		StagingBufferVk stagingBuffer(m_pRenderDevice, stagingBufferCI);
		if (!stagingBuffer.IsValid())
		{
			LogError("Failed to copy data to texture. Could not create staging buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		res = stagingBuffer.CopyData(data, sizeBytes);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to texture! Could not copy data into staging buffer");
			return res;
		}

		// Transition layout to TRANSFER_DST
		res = TransitionImageLayout(textureVk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to texture! Could not transition image layout to TRANSFER_DST_OPTIMAL!");
			return res;
		}

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = TEX_UTILS::ConvertAspectFlags(textureVk->GetAspectFlags());
		copyRegion.imageSubresource.mipLevel = 0; // TODO - Expose as parameter when mip levels are implemented
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = textureVk->GetArrayLayers();

		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent = { textureVk->GetWidth(), textureVk->GetHeight(), 1 };

		vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer.GetBuffer(), textureVk->GetBaseImage(), textureVk->GetLayout(), 1, &copyRegion);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::GetOrCreateCommandBuffer(QUEUE_TYPE type, bool isPrimaryCmdBuffer, VkCommandBuffer& out_cmdBuffer)
	{
		ASSERT_MSG(out_cmdBuffer == VK_NULL_HANDLE, "Called GetOrCreateCommandBuffer() but out_cmdBuffer is not null?");

		if (m_pRenderDevice == nullptr)
		{
			LogError("Failed to create command buffer. Render device is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = GetLastCommandBuffer(type);
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = isPrimaryCmdBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocInfo.commandPool = m_pRenderDevice->GetCommandPool(type);
			allocInfo.commandBufferCount = 1;

			VkResult res = vkAllocateCommandBuffers(m_pRenderDevice->GetLogicalDevice(), &allocInfo, &cmdBuffer);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to allocate primary command buffer! Got result: \"%s\"", string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}

			// Cache it in the command buffer list, unless it's a primary command buffer
			if ((type == QUEUE_TYPE::GRAPHICS) && isPrimaryCmdBuffer)
			{
				if (m_primaryCmdBuffer != VK_NULL_HANDLE)
				{
					FreeCommandBuffer(m_primaryCmdBuffer, QUEUE_TYPE::GRAPHICS);
				}
				m_primaryCmdBuffer = cmdBuffer;
			}
			else
			{
				CommandBufferList& cmdBuffList = m_cmdBuffers[static_cast<u32>(type)];
				cmdBuffList.push_back(cmdBuffer);
			}

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // TODO - Determine how to use correct value
			vkBeginCommandBuffer(cmdBuffer, &beginInfo);

			out_cmdBuffer = cmdBuffer;
		}

		out_cmdBuffer = cmdBuffer;
		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::FreeCommandBuffer(VkCommandBuffer cmdBuffer, QUEUE_TYPE queueType)
	{
		VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(queueType);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), cmdPool, 1, &cmdBuffer);
	}

	void DeviceContextVk::FreeCachedCommandBuffers()
	{
		// Free secondary command buffers
		FreeSecondaryCommandBuffers();

		// Free primary command buffer (allocated from graphics pool by definition)
		VkCommandPool graphicsCmdPool = m_pRenderDevice->GetCommandPool(QUEUE_TYPE::GRAPHICS);
		vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), graphicsCmdPool, 1, &m_primaryCmdBuffer);
		m_primaryCmdBuffer = VK_NULL_HANDLE;
	}

	void DeviceContextVk::FreeSecondaryCommandBuffers()
	{
		ASSERT_MSG(m_cmdBuffers.size() == static_cast<size_t>(QUEUE_TYPE::COUNT), "Command list container doesn't have space to hold command lists from every queue type");

		VkDevice logicalDevice = m_pRenderDevice->GetLogicalDevice();
		for (u32 i = 0; i < static_cast<u32>(QUEUE_TYPE::COUNT); i++)
		{
			CommandBufferList& cmdList = m_cmdBuffers[i];
			if (cmdList.size() <= 0)
			{
				// Nothing to free
				return;
			}

			VkCommandPool cmdPool = m_pRenderDevice->GetCommandPool(static_cast<QUEUE_TYPE>(i));
			vkFreeCommandBuffers(logicalDevice, cmdPool, static_cast<u32>(cmdList.size()), cmdList.data());
			cmdList.clear();
		}
	}

	VkCommandBuffer DeviceContextVk::GetLastCommandBuffer(QUEUE_TYPE queueType)
	{
		ASSERT_MSG(m_cmdBuffers.size() == static_cast<size_t>(QUEUE_TYPE::COUNT), "Command list container doesn't have space to hold command lists from every queue type");

		if (queueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to get last command buffer. Requested queue type is invalid: \"COUNT\"!");
			return VK_NULL_HANDLE;
		}

		const CommandBufferList& cmdList = m_cmdBuffers.at(static_cast<u32>(queueType));
		if (cmdList.size() <= 0)
		{
			return VK_NULL_HANDLE;
		}

		return cmdList.back();
	}

	QUEUE_TYPE DeviceContextVk::GetQueueTypeFromPipelineBindPoint(VkPipelineBindPoint vkBindPoint)
	{
		QUEUE_TYPE cmdQueueType = QUEUE_TYPE::COUNT;
		switch (vkBindPoint)
		{
		case VK_PIPELINE_BIND_POINT_GRAPHICS:
		{
			cmdQueueType = QUEUE_TYPE::GRAPHICS;
			break;
		}
		case VK_PIPELINE_BIND_POINT_COMPUTE:
		{
			cmdQueueType = QUEUE_TYPE::COMPUTE;
			break;
		}
		default:
		{
			break;
		}
		}

		return cmdQueueType;
	}

	STATUS_CODE DeviceContextVk::FlushInternal(QUEUE_TYPE queueType, const VkCommandBuffer* pCommandBuffers, u32 commandBufferCount, const FlushSyncData& syncData)
	{
		VkPipelineStageFlags waitDstFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		VkSubmitInfo vkSubmitInfo{};
		vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		vkSubmitInfo.waitSemaphoreCount = syncData.waitSemaphoreCount;
		vkSubmitInfo.pWaitSemaphores = syncData.pWaitSemaphores;
		vkSubmitInfo.pWaitDstStageMask = &waitDstFlags;
		vkSubmitInfo.commandBufferCount = commandBufferCount;
		vkSubmitInfo.pCommandBuffers = pCommandBuffers;
		vkSubmitInfo.signalSemaphoreCount = 0; // TODO
		vkSubmitInfo.pSignalSemaphores = nullptr; // TODO

		VkQueue queue = m_pRenderDevice->GetQueue(queueType);
		VkResult res = vkQueueSubmit(queue, 1, &vkSubmitInfo, syncData.signalFence);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to flush command buffers! Submit call failed with error: %s", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		// TODO - Remove this waitIdle call, prefer synchronization from render graph instead
		res = vkQueueWaitIdle(queue);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to wait until queue became idle after submitting! Got error: %s", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}
}