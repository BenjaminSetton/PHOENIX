
#include <vulkan/vk_enum_string_helper.h>

#include "device_context_vk.h"

#include "acceleration_structure_vk.h"
#include "buffer_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "swap_chain_vk.h"
#include "uniform_vk.h"
#include "utils/buffer_utils.h"
#include "utils/buffer_type_converter.h"
#include "utils/logger.h"
#include "utils/sanity.h"
#include "utils/staging_buffer.h"
#include "utils/texture_type_converter.h"
#include "utils/pipeline_type_converter.h"

#define VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, msg) if(cmdBuffer == VK_NULL_HANDLE) { LogError(msg); return STATUS_CODE::ERR_INTERNAL; }

namespace PHX
{
	DeviceContextVk::DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo) : m_pRenderDevice(nullptr),
		m_cmdBuffers(), m_wasWorkSubmitted(true), m_contextualPipeline(nullptr)
	{
		UNUSED(createInfo);

		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create a device context, but the render device is null!");
			return;
		}
		m_pRenderDevice = pRenderDevice;
	}

	DeviceContextVk::~DeviceContextVk()
	{
		DeallocateCommandBuffers();
	}

	STATUS_CODE DeviceContextVk::BindVertexBuffer(BufferHandle vertexBuffer)
	{
		BufferVk* vBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(vertexBuffer));
		if (vBufferVk == nullptr)
		{
			LogError("Failed to bind vertex buffer! Vertex buffer is null");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bind vertex buffer! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkBuffer vkBuffer = vBufferVk->GetBuffer();
		VkDeviceSize offset = vBufferVk->GetOffset();

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkBuffer, &offset);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer, INDEX_TYPE indexType)
	{
		BufferVk* vBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(vertexBuffer));
		if (vBufferVk == nullptr)
		{
			LogError("Failed to bind mesh! Vertex buffer is null");
			return STATUS_CODE::ERR_API;
		}

		BufferVk* iBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(indexBuffer));
		if (iBufferVk == nullptr)
		{
			LogError("Failed to bind mesh! Index buffer is null");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bind mesh! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkBuffer vkBuffer = vBufferVk->GetBuffer();
		VkDeviceSize offset = vBufferVk->GetOffset();

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vkBuffer, &offset);
		vkCmdBindIndexBuffer(cmdBuffer, iBufferVk->GetBuffer(), 0, BUFFER_UTILS::ConvertIndexType(indexType));
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BindUniformCollection(UniformCollectionHandle uniformCollection)
	{
		UniformCollectionVk* uniformCollectionVk = static_cast<UniformCollectionVk*>(m_pRenderDevice->ResolveHandle(uniformCollection));
		if (uniformCollectionVk == nullptr)
		{
			LogError("Attempting to bind uniform collection but the uniform collection is invalid!");
			return STATUS_CODE::ERR_API;
		}

		if (m_contextualPipeline == nullptr)
		{
			LogError("Failed to bind uniform collection. Pipeline is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		
		QUEUE_TYPE cmdQueueType = GetQueueTypeFromBindPoint(m_contextualPipeline->GetBindPoint());
		if (cmdQueueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to bind uniform collection. Could not convert from bind point %u to queue type!", static_cast<u32>(m_contextualPipeline->GetBindPoint()));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(cmdQueueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bind uniform collection! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// TODO - Consider switching to a contextual pipeline object instead of just the bind point, mainly because of GetLayout() below
		vkCmdBindDescriptorSets(cmdBuffer, m_contextualPipeline->GetBindPoint(), m_contextualPipeline->GetLayout(), 0, uniformCollectionVk->GetDescriptorSetCount(), uniformCollectionVk->GetDescriptorSets(), 0, nullptr);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::SetViewport(Vec2u size, Vec2u offset)
	{
		if (size.GetX() == 0 && size.GetY() == 0)
		{
			LogWarning("Attempting to set viewport with a size of 0!");
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to set viewport! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

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

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to set scissor! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkRect2D scissor{};
		scissor.offset = { static_cast<int>(offset.GetX()), static_cast<int>(offset.GetY()) };
		scissor.extent = { size.GetX(), size.GetY() };
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Draw(u32 vertexCount)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue draw call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue draw indexed call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdDrawIndexed(cmdBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 instanceOffset)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue draw indexed instanced call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdDrawIndexed(cmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, instanceOffset);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::Dispatch(Vec3u dimensions)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::COMPUTE, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue dispatch call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdDispatch(cmdBuffer, dimensions.GetX(), dimensions.GetY(), dimensions.GetZ());
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::TraceRays(Vec3u dimensions)
	{
		ASSERT_PTR(m_contextualPipeline);

		QUEUE_TYPE cmdQueueType = GetQueueTypeFromBindPoint(m_contextualPipeline->GetBindPoint());
		if (cmdQueueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to issue trace rays call! Could not convert bind point to queue type");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(cmdQueueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue trace rays call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pRenderDevice->CmdTraceRaysKHR(
			cmdBuffer,
			m_contextualPipeline->GetRayGenSBTRegion(),
			m_contextualPipeline->GetMissSBTRegion(),
			m_contextualPipeline->GetHitSBTRegion(),
			m_contextualPipeline->GetCallableSBTRegion(),
			dimensions.GetX(),
			dimensions.GetY(),
			dimensions.GetZ()
		);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BuildBottomLevelAccelerationStructure(AccelerationStructureHandle handle)
	{
		ASSERT_MSG(m_pRenderDevice->IsRayTracingSupported(), "Failed to build acceleration structure. Ray tracing is not supported on this device!");

		AccelerationStructureVk* pAS = static_cast<AccelerationStructureVk*>(m_pRenderDevice->ResolveHandle(handle));
		if (pAS == nullptr)
		{
			LogError("Failed to build acceleration structure. Handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		if (pAS->GetType() != ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL)
		{
			LogError("Failed to build acceleration structure. The acceleration structure is not a bottom-level acceleration structure!");
			return STATUS_CODE::ERR_API;
		}

		BufferData& scratchBuffer = pAS->GetScratchBuffer();
		if (!scratchBuffer.isValid)
		{
			LogError("Failed to build acceleration structure. Scratch buffer is invalid!");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to build acceleration structure. Could not get or create command buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		const u32 geometryCount = pAS->GetGeometryCount();
		const GeometryData* pGeometries = pAS->GetGeometries();

		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
		geometries.reserve(geometryCount);
		buildRangeInfos.reserve(geometryCount);

		auto GetBufferAddress = [&](VkBuffer buffer) -> VkDeviceAddress
		{
			VkBufferDeviceAddressInfo addressInfo{};
			addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			addressInfo.buffer = buffer;
			return m_pRenderDevice->GetBufferDeviceAddressKHR(&addressInfo);
		};

		for (u32 i = 0; i < geometryCount; i++)
		{
			const GeometryData& geometry = pGeometries[i];
			VkAccelerationStructureGeometryKHR asGeometry{};
			asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			asGeometry.geometryType = geometry.type == GEOMETRY_TYPE::TRIANGLES ? VK_GEOMETRY_TYPE_TRIANGLES_KHR : VK_GEOMETRY_TYPE_AABBS_KHR;
			asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

			if (geometry.type == GEOMETRY_TYPE::TRIANGLES)
			{
				BufferVk* pVertexBuffer = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(geometry.vertexBuffer));
				if (pVertexBuffer == nullptr)
				{
					LogError("Failed to build acceleration structure. Vertex buffer is invalid!");
					return STATUS_CODE::ERR_API;
				}

				BufferVk* pIndexBuffer = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(geometry.indexBuffer));
				if (pIndexBuffer == nullptr)
				{
					LogError("Failed to build acceleration structure. Index buffer is invalid!");
					return STATUS_CODE::ERR_API;
				}

				const VkIndexType indexType = geometry.indexType == INDEX_TYPE::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

				asGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
				asGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				asGeometry.geometry.triangles.vertexData.deviceAddress = GetBufferAddress(pVertexBuffer->GetBuffer());
				asGeometry.geometry.triangles.vertexStride = geometry.vertexStride;
				asGeometry.geometry.triangles.maxVertex = geometry.vertexCount;
				asGeometry.geometry.triangles.indexType = indexType;
				asGeometry.geometry.triangles.indexData.deviceAddress = GetBufferAddress(pIndexBuffer->GetBuffer());

				VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
				rangeInfo.primitiveCount = geometry.indexBuffer.IsValid() ? (geometry.indexCount / 3) : (geometry.vertexCount / 3);
				rangeInfo.primitiveOffset = 0;
				rangeInfo.firstVertex = 0;
				rangeInfo.transformOffset = 0;
				buildRangeInfos.push_back(rangeInfo);
			}
			else if (geometry.type == GEOMETRY_TYPE::AABBS)
			{
				BufferVk* pAABBBuffer = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(geometry.aabbBuffer));
				if (pAABBBuffer == nullptr)
				{
					LogError("Failed to build acceleration structure. AABB buffer is invalid!");
					return STATUS_CODE::ERR_API;
				}

				asGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
				asGeometry.geometry.aabbs.data.deviceAddress = GetBufferAddress(pAABBBuffer->GetBuffer());
				asGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

				VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
				rangeInfo.primitiveCount = geometry.aabbCount;
				rangeInfo.primitiveOffset = 0;
				rangeInfo.firstVertex = 0;
				rangeInfo.transformOffset = 0;
				buildRangeInfos.push_back(rangeInfo);
			}

			geometries.push_back(asGeometry);
		}

		VkDeviceAddress scratchAddress = GetBufferAddress(scratchBuffer.buffer);

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfo.flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(static_cast<u32>(pAS->GetBuildFlags()));
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.dstAccelerationStructure = pAS->GetAccelerationStructure();
		buildInfo.geometryCount = static_cast<u32>(geometries.size());
		buildInfo.pGeometries = geometries.data();
		buildInfo.scratchData.deviceAddress = scratchAddress;

		const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = buildRangeInfos.data();
		m_pRenderDevice->CmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &pBuildRangeInfos);

		pAS->SetBuilt(true);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BuildTopLevelAccelerationStructure(AccelerationStructureHandle handle, BufferHandle instanceBuffer, u32 instanceCount)
	{
		ASSERT_MSG(m_pRenderDevice->IsRayTracingSupported(), "Failed to build top-level acceleration structure. Ray tracing is not supported on this device!");

		AccelerationStructureVk* pAS = static_cast<AccelerationStructureVk*>(m_pRenderDevice->ResolveHandle(handle));
		if (pAS == nullptr)
		{
			LogError("Failed to build top-level acceleration structure. Handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		if (pAS->GetType() != ACCELERATION_STRUCTURE_TYPE::TOP_LEVEL)
		{
			LogError("Failed to build top-level acceleration structure. The acceleration structure is not a top-level acceleration structure!");
			return STATUS_CODE::ERR_API;
		}

		BufferVk* pInstanceBuffer = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(instanceBuffer));
		if (pInstanceBuffer == nullptr)
		{
			LogError("Failed to build top-level acceleration structure. Instance buffer is invalid!");
			return STATUS_CODE::ERR_API;
		}

		BufferData& scratchBuffer = pAS->GetScratchBuffer();
		if (!scratchBuffer.isValid)
		{
			LogError("Failed to build top-level acceleration structure. Scratch buffer is invalid!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to build top-level acceleration structure. Could not get or create command buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		auto GetBufferAddress = [&](VkBuffer buffer) -> VkDeviceAddress
		{
			VkBufferDeviceAddressInfo addressInfo{};
			addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
			addressInfo.buffer = buffer;
			return m_pRenderDevice->GetBufferDeviceAddressKHR(&addressInfo);
		};

		VkDeviceAddress instanceAddress = GetBufferAddress(pInstanceBuffer->GetBuffer());

		VkAccelerationStructureGeometryKHR geometry{};
		geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		geometry.geometry.instances.arrayOfPointers = VK_FALSE;
		geometry.geometry.instances.data.deviceAddress = instanceAddress;

		VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
		rangeInfo.primitiveCount = instanceCount;
		rangeInfo.primitiveOffset = 0;
		rangeInfo.firstVertex = 0;
		rangeInfo.transformOffset = 0;

		VkDeviceAddress scratchAddress = GetBufferAddress(scratchBuffer.buffer);

		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(static_cast<u32>(pAS->GetBuildFlags()));
		buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.dstAccelerationStructure = pAS->GetAccelerationStructure();
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &geometry;
		buildInfo.scratchData.deviceAddress = scratchAddress;

		const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;
		m_pRenderDevice->CmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &pRangeInfo);

		pAS->SetBuilt(true);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes)
	{
		if (data == nullptr)
		{
			LogError("Failed to copy data to buffer. Data pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		if (sizeBytes <= 0)
		{
			LogError("Failed to copy data to buffer. Size is 0!");
			return STATUS_CODE::ERR_API;
		}

		BufferVk* bufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(buffer));
		if (bufferVk == nullptr)
		{
			LogError("Failed to copy data to buffer. Buffer is null!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res = STATUS_CODE::SUCCESS;

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
			StagingBufferVk* pStagingBuffer = CreateStagingBuffer(stagingBufferCI);
			if (!pStagingBuffer->IsValid())
			{
				LogError("Failed to copy data to buffer. Could not create staging buffer!");
				return STATUS_CODE::ERR_INTERNAL;
			}

			VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
			const QUEUE_TYPE transferQueueType = QUEUE_TYPE::TRANSFER;

			res = GetOrCreateCommandBuffer(transferQueueType, cmdBuffer);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to copy data to buffer! Command buffer creation failed");
				return res;
			}

			res = pStagingBuffer->CopyData(data, sizeBytes);
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
			vkCmdCopyBuffer(cmdBuffer, pStagingBuffer->GetBuffer(), bufferVk->GetBuffer(), 1, &copyRegion);
		}

		return res;
	}

	STATUS_CODE DeviceContextVk::CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes)
	{
		TextureVk* textureVk = static_cast<TextureVk*>(m_pRenderDevice->ResolveHandle(texture));
		if (textureVk == nullptr)
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

		res = GetOrCreateCommandBuffer(transferQueueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to texture! Command buffer creation failed");
			return res;
		}

		// Create a staging buffer and copy data into staging buffer
		BufferCreateInfo stagingBufferCI{}; // Buffer usage is unused in this case
		stagingBufferCI.sizeBytes = sizeBytes;
		StagingBufferVk* pStagingBuffer = CreateStagingBuffer(stagingBufferCI);
		ASSERT_PTR(pStagingBuffer);
		if (!pStagingBuffer->IsValid())
		{
			LogError("Failed to copy data to texture. Could not create staging buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		res = pStagingBuffer->CopyData(data, sizeBytes);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to copy data to texture! Could not copy data into staging buffer");
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

		vkCmdCopyBufferToImage(cmdBuffer, pStagingBuffer->GetBuffer(), textureVk->GetBaseImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BeginFrame(SwapChainVk* pSwapChain, u32 frameIndex)
	{
		if (m_pRenderDevice == nullptr)
		{
			LogError("Failed to begin frame! Render device is null");
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (pSwapChain == nullptr)
		{
			LogError("Failed to begin frame! Swap chain pointer is null");
			return STATUS_CODE::ERR_INTERNAL;
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
		res = pSwapChain->AcquireNextImage(imageAvailableSemaphore);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Swap chain could not acquire next image");
			return res;
		}

		// Only reset the fence if we're submitting work, otherwise we might deadlock
		if (m_wasWorkSubmitted)
		{
			vkResetFences(m_pRenderDevice->GetLogicalDevice(), 1, &inFlightFence);
		}

		m_wasWorkSubmitted = false;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndFrame(u32 frameIndex)
	{
		// Flush the recorded commands from all queues
		STATUS_CODE flushRes = STATUS_CODE::SUCCESS;
		for (u32 i = 0; i < static_cast<u32>(QUEUE_TYPE::COUNT); i++)
		{
			QUEUE_TYPE currQueue = static_cast<QUEUE_TYPE>(i);

			// HACK - Use hard-coded sync data for the graphics queue. Later we should probably switch
			//        to storing this information per queue
			if (currQueue == QUEUE_TYPE::GRAPHICS)
			{
				VkFence inFlightFence = m_pRenderDevice->GetInFlightFence(frameIndex);
				VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(frameIndex);

				FlushSyncData syncData{};
				syncData.pWaitSemaphores = &imageAvailableSemaphore;
				syncData.waitSemaphoreCount = 1;
				syncData.signalFence = inFlightFence;

				flushRes = Flush(currQueue, syncData);
				if (flushRes != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to end frame. Could not flush queue at index %u!", i);
					break;
				}
			}
			else
			{
				// No sync data for now
				FlushSyncData syncData{};
				flushRes = Flush(currQueue, syncData);
				if (flushRes != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to end frame. Could not flush queue at index %u!", i);
					break;
				}
			}
		}

		// Free allocated memory for command buffers and any resources linked to the command buffers
		DestroyStagingBuffers();
		DeallocateCommandBuffers();

		if (flushRes == STATUS_CODE::SUCCESS)
		{
			m_wasWorkSubmitted = true;
		}

		return flushRes;
	}

	STATUS_CODE DeviceContextVk::Flush(QUEUE_TYPE queueType, const FlushSyncData& syncData)
	{
		CommandBufferList& cmdBufferList = m_cmdBuffers.at(static_cast<u32>(queueType));
		if (cmdBufferList.size() <= 0)
		{
			// Nothing to flush
			return STATUS_CODE::SUCCESS;
		}

		VkCommandBuffer* buffersToSubmit = cmdBufferList.data();
		u32 buffersToSubmitCount = static_cast<u32>(cmdBufferList.size());

		// End recording for all command buffers that will be submitted
		for (u32 i = 0; i < buffersToSubmitCount; i++)
		{
			VkCommandBuffer& cmdBuffer = buffersToSubmit[i];
			vkEndCommandBuffer(cmdBuffer);
		}

		// Submit all commands
		STATUS_CODE res = FlushInternal(queueType, buffersToSubmit, buffersToSubmitCount, syncData);

		return res;
	}

	STATUS_CODE DeviceContextVk::BeginRenderPass(VkRenderPass renderPass, FramebufferVk* pFramebuffer, ClearValues* pClearColors, u32 clearColorCount)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin render pass! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Process clear values
		std::vector<VkClearValue> vkClearValues(clearColorCount);
		for (u32 i = 0; i < clearColorCount; i++)
		{
			VkClearValue clearValues{};
			if (pClearColors[i].useClearColor)
			{
				memcpy(&clearValues.color.float32, &pClearColors[i].color.color, sizeof(Vec4f));
			}
			else
			{
				clearValues.depthStencil.depth = pClearColors[i].depthStencil.depthClear;
				clearValues.depthStencil.stencil = pClearColors[i].depthStencil.stencilClear;
			}

			vkClearValues[i] = clearValues;
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = pFramebuffer->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { pFramebuffer->GetWidth(), pFramebuffer->GetHeight() };
		renderPassInfo.clearValueCount = clearColorCount;
		renderPassInfo.pClearValues = (pClearColors == nullptr) ? nullptr : vkClearValues.data();
		vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndRenderPass()
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end render pass! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdEndRenderPass(cmdBuffer);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::InsertImageMemoryBarrier(TextureVk* pTexture, QUEUE_TYPE queueType, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		if (pTexture == nullptr)
		{
			LogError("Failed to insert image memory barrier. Texture pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		if (pTexture->GetBaseImage() == nullptr)
		{
			LogError("Failed to insert image memory barrier. Texture base image is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer;
		STATUS_CODE res = GetOrCreateCommandBuffer(queueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to insert image memory barrier. Could not retrive or create a valid command buffer!");
			return res;
		}

		VkImageMemoryBarrier imageBarrier;
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.pNext = nullptr;
		imageBarrier.oldLayout = oldLayout;
		imageBarrier.newLayout = newLayout;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.image = pTexture->GetBaseImage();
		imageBarrier.subresourceRange.aspectMask = TEX_UTILS::ConvertAspectFlags(pTexture->GetAspectFlags());
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.levelCount = pTexture->GetMipLevels();
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount = pTexture->GetArrayLayers();
		imageBarrier.srcAccessMask = srcAccessMask;
		imageBarrier.dstAccessMask = dstAccessMask;

		vkCmdPipelineBarrier(
			cmdBuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr, // No memory barriers
			0, nullptr, // No buffer barriers
			1, &imageBarrier
		);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::InsertBufferMemoryBarrier(BufferVk* pBuffer, QUEUE_TYPE queueType, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
	{
		if (pBuffer == nullptr)
		{
			LogError("Failed to insert buffer memory barrier. Buffer pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer;
		STATUS_CODE res = GetOrCreateCommandBuffer(queueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to insert buffer memory barrier. Could not retrive or create a valid command buffer!");
			return res;
		}

		VkBufferMemoryBarrier bufferBarrier;
		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.pNext = nullptr;
		bufferBarrier.srcAccessMask = srcAccessMask;
		bufferBarrier.dstAccessMask = dstAccessMask;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = pBuffer->GetBuffer();
		bufferBarrier.offset = 0;
		bufferBarrier.size = pBuffer->GetSize();

		vkCmdPipelineBarrier(
			cmdBuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr, // No memory barriers
			1, &bufferBarrier,
			0, nullptr	// No image barriers
		);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::GetOrCreateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer)
	{
		if (m_pRenderDevice == nullptr)
		{
			LogError("Failed to create command buffer. Render device is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// HACK - Reroute all transfer commands into graphics command buffer
		if (type == QUEUE_TYPE::TRANSFER)
		{
			type = QUEUE_TYPE::GRAPHICS;
		}

		CommandBufferList& cmdList = m_cmdBuffers.at(static_cast<u32>(type));
		if (cmdList.size() <= 0)
		{
			// Command list doesn't have a command buffer already, so make a new one
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = m_pRenderDevice->GetCommandPool(type);
			allocInfo.commandBufferCount = 1;

			VkResult res = vkAllocateCommandBuffers(m_pRenderDevice->GetLogicalDevice(), &allocInfo, &out_cmdBuffer);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to allocate command buffer for queue type %u! Got result: \"%s\"", static_cast<u32>(type), string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}

			// Cache it in the command buffer list
			cmdList.push_back(out_cmdBuffer);

			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = 0;
			res = vkBeginCommandBuffer(out_cmdBuffer, &beginInfo);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to allocate command buffer for queue type %u! Got result: \"%s\"", static_cast<u32>(type), string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}
		}
		else
		{
			// Found an existing command buffer, use that one
			out_cmdBuffer = cmdList.back();
		}

		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::DeallocateCommandBuffers()
	{
		for (u32 i = 0; i < static_cast<u32>(QUEUE_TYPE::COUNT); i++)
		{
			const QUEUE_TYPE currQueueType = static_cast<QUEUE_TYPE>(i);
			VkCommandPool currPool = m_pRenderDevice->GetCommandPool(currQueueType);
			if (currPool == VK_NULL_HANDLE)
			{
				// Not all queue types have a command pool (e.g. PRESENT)
				continue;
			}

			// TODO - Reuse command buffers
			CommandBufferList& cmdBufferList = m_cmdBuffers[i];
			if (!cmdBufferList.empty())
			{
				vkFreeCommandBuffers(m_pRenderDevice->GetLogicalDevice(), currPool, static_cast<u32>(cmdBufferList.size()), cmdBufferList.data());
			}

			vkResetCommandPool(m_pRenderDevice->GetLogicalDevice(), currPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
			cmdBufferList.clear();
		}
	}

	QUEUE_TYPE DeviceContextVk::GetQueueTypeFromBindPoint(VkPipelineBindPoint bindPoint)
	{
		QUEUE_TYPE cmdQueueType = QUEUE_TYPE::COUNT;
		switch (bindPoint)
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
		case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
		{
			cmdQueueType = QUEUE_TYPE::GRAPHICS;
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

	StagingBufferVk* DeviceContextVk::CreateStagingBuffer(const BufferCreateInfo& createInfo)
	{
		StagingBufferVk* stagingBuffer = new StagingBufferVk(m_pRenderDevice, createInfo);
		m_stagingBuffers.push_back(stagingBuffer);

		return stagingBuffer;
	}

	void DeviceContextVk::DestroyStagingBuffers()
	{
		for (u32 i = 0; i < static_cast<u32>(m_stagingBuffers.size()); i++)
		{
			delete m_stagingBuffers[i];
		}
		m_stagingBuffers.clear();
	}

	STATUS_CODE DeviceContextVk::SetContextualPipeline(PipelineVk* pPipeline)
	{
		if (pPipeline == nullptr)
		{
			LogWarning("Failed to bind pipeline. Pipeline is null!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Command buffer
		QUEUE_TYPE cmdQueueType = GetQueueTypeFromBindPoint(pPipeline->GetBindPoint());
		if (cmdQueueType == QUEUE_TYPE::COUNT)
		{
			LogError("Failed to bind pipeline. Could not convert from bind point %s to queue type!", string_VkPipelineBindPoint(pPipeline->GetBindPoint()));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(cmdQueueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bind pipeline! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdBindPipeline(cmdBuffer, pPipeline->GetBindPoint(), pPipeline->GetPipeline());

		// Cache the contextual pipeline so other calls can reference it. This should be cleared in ResetContextualPipeline
		m_contextualPipeline = pPipeline;
		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::ResetContextualPipeline()
	{
		m_contextualPipeline = nullptr;
	}
}