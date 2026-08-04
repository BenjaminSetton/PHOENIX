
#include <algorithm>
#include <vulkan/vk_enum_string_helper.h>

#include "device_context_vk.h"

#include "PHX/types/acceleration_structure_desc.h"
#include "acceleration_structure_vk.h"
#include "buffer_vk.h"
#include "framebuffer_vk.h"
#include "pipeline_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "uniform_vk.h"
#include "utils/acceleration_structure_utils.h"
#include "utils/buffer_utils.h"
#include "utils/buffer_type_converter.h"
#include "utils/debug_utils.h"
#include "utils/logger.h"
#include "utils/sanity.h"
#include "utils/texture_type_converter.h"
#include "utils/pipeline_type_converter.h"

STATIC_ASSERT_MSG(sizeof(PHX::AccelerationStructureInstance) == sizeof(VkAccelerationStructureInstanceKHR), "PHX::AccelerationStructureInstance size mismatch with VkAccelerationStructureInstanceKHR");
STATIC_ASSERT_MSG(alignof(PHX::AccelerationStructureInstance) == alignof(VkAccelerationStructureInstanceKHR), "PHX::AccelerationStructureInstance alignment mismatch with VkAccelerationStructureInstanceKHR");

#define VERIFY_CMD_BUF_RETURN_ERR(cmdBuffer, msg) if(cmdBuffer == VK_NULL_HANDLE) { LogError(msg); return STATUS_CODE::ERR_INTERNAL; }

namespace PHX
{
	DeviceContextVk::DeviceContextVk(RenderDeviceVk* pRenderDevice, const DeviceContextCreateInfo& createInfo) : m_pRenderDevice(nullptr),
		m_submissionBatches(), m_chainSemaphores(), m_stagingPool(pRenderDevice), m_workSubmitted(true), m_assignedFrameIndex(0), m_contextualPipeline(nullptr),
		m_pMetrics(nullptr), m_queryPool(VK_NULL_HANDLE), m_queryFrameBaseIndex(0), m_beginTimestampWritten(false)
	{
		UNUSED(createInfo);

		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create a device context, but the render device is null!");
			return;
		}
		m_pRenderDevice = pRenderDevice;

		// Initialize to true so that BeginFrame waits on (and resets) the frame fence on the first frame.
		// Fences are created in the signaled state, so the initial wait returns immediately.
		m_workSubmitted = true;

		m_assignedFrameIndex = createInfo.assignedFrameIndex;
	}

	DeviceContextVk::~DeviceContextVk()
	{
		DeallocateCommandBuffers();
		DestroyChainSemaphores();
		m_stagingPool.Destroy();
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
		const VkDescriptorSet* descriptorSets = uniformCollectionVk->GetDescriptorSets(m_assignedFrameIndex);
		vkCmdBindDescriptorSets(cmdBuffer, m_contextualPipeline->GetBindPoint(), m_contextualPipeline->GetLayout(), 0, uniformCollectionVk->GetDescriptorSetCount(m_assignedFrameIndex), descriptorSets, 0, nullptr);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::FlushUniformUpdates(UniformCollectionHandle uniformCollection)
	{
		UniformCollectionVk* uniformCollectionVk = static_cast<UniformCollectionVk*>(m_pRenderDevice->ResolveHandle(uniformCollection));
		if (uniformCollectionVk == nullptr)
		{
			LogError("Attempting to flush uniform updates but the uniform collection is invalid!");
			return STATUS_CODE::ERR_API;
		}

		if (m_pMetrics)
		{
			m_pMetrics->uniformUpdates++;
		}

		return uniformCollectionVk->Flush(m_assignedFrameIndex);
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

		if (m_pMetrics)
		{
			m_pMetrics->drawCalls++;
			m_pMetrics->vertices += vertexCount;
			m_pMetrics->triangles += vertexCount / 3;
		}

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

		if (m_pMetrics)
		{
			m_pMetrics->drawCalls++;
			m_pMetrics->indices += indexCount;
			m_pMetrics->triangles += indexCount / 3;
		}

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

		if (m_pMetrics)
		{
			m_pMetrics->drawCalls++;
			m_pMetrics->indices += indexCount * instanceCount;
			m_pMetrics->triangles += (indexCount * instanceCount) / 3;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexedIndirect(BufferHandle argsBuffer, u32 drawCount, u32 stride, u64 argsOffset)
	{
		BufferVk* argsBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(argsBuffer));
		if (argsBufferVk == nullptr)
		{
			LogError("Failed to issue draw indexed indirect call. Args buffer is null!");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue draw indexed indirect call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		vkCmdDrawIndexedIndirect(cmdBuffer, argsBufferVk->GetBuffer(), argsBufferVk->GetOffset() + argsOffset, drawCount, stride);

		if (m_pMetrics)
		{
			m_pMetrics->drawCalls += drawCount;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::DrawIndexedIndirectCount(BufferHandle argsBuffer, u64 argsOffset, BufferHandle countBuffer, u64 countOffset, u32 maxDrawCount, u32 stride)
	{
		BufferVk* argsBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(argsBuffer));
		if (argsBufferVk == nullptr)
		{
			LogError("Failed to issue draw indexed indirect count call. Args buffer is null!");
			return STATUS_CODE::ERR_API;
		}

		BufferVk* countBufferVk = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(countBuffer));
		if (countBufferVk == nullptr)
		{
			LogError("Failed to issue draw indexed indirect count call. Count buffer is null!");
			return STATUS_CODE::ERR_API;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(QUEUE_TYPE::GRAPHICS, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to issue draw indexed indirect count call! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_pRenderDevice->CmdDrawIndexedIndirectCount(
			cmdBuffer,
			argsBufferVk->GetBuffer(), argsBufferVk->GetOffset() + argsOffset,
			countBufferVk->GetBuffer(), countBufferVk->GetOffset() + countOffset,
			maxDrawCount, stride);

		if (m_pMetrics)
		{
			m_pMetrics->drawCalls += maxDrawCount; // Approximate — actual count is GPU-determined
		}

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
			asGeometry.flags = ConvertGeometryFlags(geometry.flags);

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
				asGeometry.geometry.triangles.vertexData.deviceAddress = GetBufferAddress(pVertexBuffer->GetBuffer()) + static_cast<VkDeviceAddress>(geometry.firstVertex) * geometry.vertexStride;
				asGeometry.geometry.triangles.vertexStride = geometry.vertexStride;
				asGeometry.geometry.triangles.maxVertex = geometry.vertexCount > 0 ? geometry.vertexCount - 1 : 0;
				asGeometry.geometry.triangles.indexType = indexType;
				asGeometry.geometry.triangles.indexData.deviceAddress = GetBufferAddress(pIndexBuffer->GetBuffer()) + geometry.indexByteOffset;

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
				rangeInfo.firstVertex = geometry.firstVertex;
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
			StagingAllocation stagingAlloc = AllocateStaging(sizeBytes);
			if (!stagingAlloc.isValid)
			{
				LogError("Failed to copy data to buffer. Could not allocate staging memory!");
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

			memcpy(stagingAlloc.mappedData, data, sizeBytes);

			// Copy from staging buffer to GPU buffer
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = stagingAlloc.offset;
			copyRegion.dstOffset = 0;
			copyRegion.size = sizeBytes;
			vkCmdCopyBuffer(cmdBuffer, stagingAlloc.buffer, bufferVk->GetBuffer(), 1, &copyRegion);
		}

		return res;
	}

	STATUS_CODE DeviceContextVk::CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes, u32 mipLevel)
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

		// Sub-allocate from staging pool and copy data
		StagingAllocation stagingAlloc = AllocateStaging(sizeBytes);
		if (!stagingAlloc.isValid)
		{
			LogError("Failed to copy data to texture. Could not allocate staging memory!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		memcpy(stagingAlloc.mappedData, data, sizeBytes);

		// Calculate mip-level dimensions from the base texture size
		const u32 mipWidth = std::max(1u, textureVk->GetWidth() >> mipLevel);
		const u32 mipHeight = std::max(1u, textureVk->GetHeight() >> mipLevel);

		VkBufferImageCopy copyRegion{};
		copyRegion.bufferOffset = stagingAlloc.offset;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;

		copyRegion.imageSubresource.aspectMask = TEX_UTILS::ConvertAspectFlags(textureVk->GetAspectFlags());
		copyRegion.imageSubresource.mipLevel = mipLevel;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = textureVk->GetArrayLayers();

		copyRegion.imageOffset = { 0, 0, 0 };
		copyRegion.imageExtent = { mipWidth, mipHeight, 1 };

		vkCmdCopyBufferToImage(cmdBuffer, stagingAlloc.buffer, textureVk->GetBaseImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::BeginFrame(SwapChainVk* pSwapChain)
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
		VkResult vkRes;

		// Wait on the single frame fence signaled by the last submission batch of the previous frame
		// with this index. Because batches are chained with semaphores (each waits on the previous),
		// the last batch completing guarantees that every batch — and therefore all command buffers and
		// staging memory — from that frame is done on the GPU.
		if (m_workSubmitted)
		{
			VkFence frameFence = m_pRenderDevice->GetQueueFence(QUEUE_TYPE::GRAPHICS, m_assignedFrameIndex);
			vkRes = vkWaitForFences(m_pRenderDevice->GetLogicalDevice(), 1, &frameFence, VK_TRUE, UINT64_MAX);
			if (vkRes != VK_SUCCESS)
			{
				// This should never happen and will mess up upcoming frames
				LogError("Failed to wait for frame fence, skipping frame. Got error: \"%s\"", string_VkResult(vkRes));
				return STATUS_CODE::ERR_INTERNAL;
			}

			vkRes = vkResetFences(m_pRenderDevice->GetLogicalDevice(), 1, &frameFence);
			if (vkRes != VK_SUCCESS)
			{
				LogError("Failed to reset frame fence, skipping frame. Got error: \"%s\"", string_VkResult(vkRes));
				return STATUS_CODE::ERR_INTERNAL;
			}
		}

		// Reset staging pool for reuse. The fence wait above guarantees the GPU is done
		// with the staging memory from the previous frame with the same index.
		ResetStagingPool();
		ResetCommandBuffers();

		VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(m_assignedFrameIndex);
		res = pSwapChain->AcquireNextImage(imageAvailableSemaphore);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame! Swap chain could not acquire next image");
			return res;
		}

		// Reset work submission tracking for the new frame
		m_workSubmitted = false;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndFrame()
	{
		const u32 batchCount = static_cast<u32>(m_submissionBatches.size());
		if (batchCount == 0)
		{
			// No work was recorded this frame; nothing to submit
			return STATUS_CODE::SUCCESS;
		}

		// Submit the batches in the order they were recorded (which is the render graph's dependency
		// order). Batches are chained together with binary semaphores so that a consumer batch on one
		// queue does not begin until its producer batch on another queue has finished:
		//
		//   - batch 0 waits on the swapchain 'image available' semaphore (nothing starts before acquire)
		//   - batch i (>0) waits on the chain semaphore signaled by batch i-1
		//   - batch i (< last) signals its own chain semaphore for the next batch to wait on
		//   - the last batch signals 'render finished' (which Present waits on) and the frame fence
		//
		// Only the last batch signals the frame fence: since every batch waits on the previous one,
		// the last batch completing implies all earlier batches are done too.
		STATUS_CODE res = EnsureChainSemaphores(batchCount > 0 ? batchCount - 1 : 0);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame. Could not create chain semaphores!");
			return res;
		}

		VkSemaphore imageAvailableSemaphore = m_pRenderDevice->GetImageAvailableSemaphore(m_assignedFrameIndex);
		VkSemaphore renderFinishedSemaphore = m_pRenderDevice->GetRenderFinishedSemaphore(m_assignedFrameIndex);
		VkFence frameFence = m_pRenderDevice->GetQueueFence(QUEUE_TYPE::GRAPHICS, m_assignedFrameIndex);

		for (u32 i = 0; i < batchCount; i++)
		{
			const SubmissionBatch& batch = m_submissionBatches[i];
			const bool isFirstBatch = (i == 0);
			const bool isLastBatch  = (i == batchCount - 1);

			vkEndCommandBuffer(batch.cmdBuffer);

			VkSemaphore waitSemaphore   = isFirstBatch ? imageAvailableSemaphore : m_chainSemaphores[i - 1];
			VkSemaphore signalSemaphore = isLastBatch  ? renderFinishedSemaphore : m_chainSemaphores[i];

			FlushSyncData syncData{};
			syncData.pWaitSemaphores     = &waitSemaphore;
			syncData.waitSemaphoreCount  = 1;
			syncData.pSignalSemaphores   = &signalSemaphore;
			syncData.signalSemaphoreCount = 1;
			syncData.signalFence         = isLastBatch ? frameFence : VK_NULL_HANDLE;

			res = FlushInternal(batch.queueType, &batch.cmdBuffer, 1, syncData);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to end frame. Could not flush submission batch %u (queue type %u)!", i, static_cast<u32>(batch.queueType));
				// If we failed to flush, we cannot delete resources because they
				// might still be in-use by the GPU
				return res;
			}
		}

		// At least one batch was submitted and the last one signaled the frame fence, so BeginFrame
		// must wait on it next time this frame index comes around.
		m_workSubmitted = true;

		return STATUS_CODE::SUCCESS;
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

	STATUS_CODE DeviceContextVk::BeginLabel(QUEUE_TYPE queueType, const char* name)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(queueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin label! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::BeginLabel(m_pRenderDevice->GetLogicalDevice(), cmdBuffer, name);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE DeviceContextVk::EndLabel(QUEUE_TYPE queueType)
	{
		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(queueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end label! Could not get or create command buffer");
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::EndLabel(m_pRenderDevice->GetLogicalDevice(), cmdBuffer);
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

	STATUS_CODE DeviceContextVk::InsertAccelerationStructureMemoryBarrier(AccelerationStructureVk* pAS, QUEUE_TYPE queueType, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
	{
		ASSERT_MSG(m_pRenderDevice->IsRayTracingSupported(), "Failed to insert acceleration structure memory barrier. Ray tracing is not supported on this device!");

		if (pAS == nullptr)
		{
			LogError("Failed to insert acceleration structure memory barrier. Acceleration structure pointer is null!");
			return STATUS_CODE::ERR_API;
		}

		BufferData& resultBuffer = pAS->GetResultBuffer();
		if (!resultBuffer.isValid)
		{
			LogError("Failed to insert acceleration structure memory barrier. Result buffer is invalid!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
		STATUS_CODE res = GetOrCreateCommandBuffer(queueType, cmdBuffer);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to insert acceleration structure memory barrier. Could not get or create command buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkBufferMemoryBarrier bufferBarrier{};
		bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferBarrier.srcAccessMask = srcAccessMask;
		bufferBarrier.dstAccessMask = dstAccessMask;
		bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferBarrier.buffer = resultBuffer.buffer;
		bufferBarrier.offset = 0;
		bufferBarrier.size = resultBuffer.size;

		vkCmdPipelineBarrier(
			cmdBuffer,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			1, &bufferBarrier,
			0, nullptr
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

		// Attempt to use an already-active command buffer from this frame
		if (TryReuseActiveCommandBuffer(type, out_cmdBuffer))
		{
			return STATUS_CODE::SUCCESS;
		}

		return AllocateCommandBuffer(type, out_cmdBuffer);
	}

	bool DeviceContextVk::TryReuseActiveCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer)
	{
		u32 familyIndex = m_pRenderDevice->GetQueueFamilyIndex(type);

		// Reuse the current (most recent) batch if it targets the same queue family. Commands
		// recorded back-to-back on the same queue family belong in a single submission
		if (!m_submissionBatches.empty() && m_submissionBatches.back().queueFamilyIndex == familyIndex)
		{
			out_cmdBuffer = m_submissionBatches.back().cmdBuffer;
			return true;
		}

		return false;
	}

	STATUS_CODE DeviceContextVk::AllocateCommandBuffer(QUEUE_TYPE type, VkCommandBuffer& out_cmdBuffer)
	{
		u32 queueType = static_cast<u32>(type);
		u32 familyIndex = m_pRenderDevice->GetQueueFamilyIndex(type);
		VkDevice device = m_pRenderDevice->GetLogicalDevice();
		VkCommandPool pool = m_pRenderDevice->GetCommandPool(type, m_assignedFrameIndex);

		// Pull from the cmd buffer cache. These are reset in bulk every frame through vkResetCommandPool()
		auto& cache = m_commandBufferCache[static_cast<u32>(type)];
		if (!cache.empty())
		{
			out_cmdBuffer = cache.back();
			cache.pop_back();
		}
		else
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = pool;
			allocInfo.commandBufferCount = 1;

			VkResult res = vkAllocateCommandBuffers(device, &allocInfo, &out_cmdBuffer);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to allocate command buffer for queue type %u! Got result: \"%s\"", queueType, string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}

			LogDebug("Allocated new command buffer for queue type %u", queueType);
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		VkResult res = vkBeginCommandBuffer(out_cmdBuffer, &beginInfo);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to begin command buffer for queue type %u! Got result: \"%s\"", queueType, string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		// If timestamp queries are enabled and this is the first graphics/compute command buffer
		// of the frame, reset the query slots and write the begin timestamp. Transfer queues don't
		// support timestamp queries, so we skip them and wait for a compatible queue
		if (!m_beginTimestampWritten && m_queryPool != VK_NULL_HANDLE &&
			(type == QUEUE_TYPE::GRAPHICS || type == QUEUE_TYPE::COMPUTE))
		{
			vkCmdResetQueryPool(out_cmdBuffer, m_queryPool, m_queryFrameBaseIndex, 2);
			vkCmdWriteTimestamp(out_cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_queryPool, m_queryFrameBaseIndex);
			m_beginTimestampWritten = true;
		}

		SubmissionBatch newBatch{};
		newBatch.queueType = type;
		newBatch.queueFamilyIndex = familyIndex;
		newBatch.cmdBuffer = out_cmdBuffer;
		m_submissionBatches.push_back(newBatch);

		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::DeallocateCommandBuffers()
	{
		// Free all cached command buffers
		VkDevice device = m_pRenderDevice->GetLogicalDevice();

		for (u32 i = 0; i < static_cast<u32>(QUEUE_TYPE::COUNT); i++)
		{
			QUEUE_TYPE queueType = static_cast<QUEUE_TYPE>(i);
			VkCommandPool pool = m_pRenderDevice->GetCommandPool(queueType, m_assignedFrameIndex);
			if (pool == VK_NULL_HANDLE)
			{
				continue;
			}

			auto& cache = m_commandBufferCache[i];
			if (!cache.empty())
			{
				vkFreeCommandBuffers(device, pool, static_cast<u32>(cache.size()), cache.data());
				cache.clear();
			}
		}

		m_submissionBatches.clear();
	}

	void DeviceContextVk::ResetCommandBuffers()
	{
		// vkResetCommandPool resets all command buffers allocated from each pool to their
		// initial state in one call. No need to individually reset each command buffer.
		VkDevice device = m_pRenderDevice->GetLogicalDevice();
		for (u32 i = 0; i < static_cast<u32>(QUEUE_TYPE::COUNT); i++)
		{
			QUEUE_TYPE queueType = static_cast<QUEUE_TYPE>(i);
			VkCommandPool pool = m_pRenderDevice->GetCommandPool(queueType, m_assignedFrameIndex);
			if (pool != VK_NULL_HANDLE)
			{
				vkResetCommandPool(device, pool, 0);
			}
		}

		// Return all command buffers from this frame's batches back to the cache for reuse.
		// BeginFrame's fence wait guarantees the GPU is done with all submissions.
		for (const SubmissionBatch& batch : m_submissionBatches)
		{
			if (batch.cmdBuffer != VK_NULL_HANDLE)
			{
				m_commandBufferCache[static_cast<u32>(batch.queueType)].push_back(batch.cmdBuffer);
			}
		}

		m_submissionBatches.clear();
	}

	STATUS_CODE DeviceContextVk::EnsureChainSemaphores(u32 count)
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for (u32 i = static_cast<u32>(m_chainSemaphores.size()); i < count; i++)
		{
			VkSemaphore semaphore = VK_NULL_HANDLE;
			VkResult res = vkCreateSemaphore(m_pRenderDevice->GetLogicalDevice(), &semaphoreInfo, nullptr, &semaphore);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to create chain semaphore! Got result: \"%s\"", string_VkResult(res));
				return STATUS_CODE::ERR_INTERNAL;
			}
			m_chainSemaphores.push_back(semaphore);
		}

		return STATUS_CODE::SUCCESS;
	}

	void DeviceContextVk::DestroyChainSemaphores()
	{
		if (m_pRenderDevice == nullptr)
		{
			return;
		}

		for (VkSemaphore semaphore : m_chainSemaphores)
		{
			if (semaphore != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(m_pRenderDevice->GetLogicalDevice(), semaphore, nullptr);
			}
		}
		m_chainSemaphores.clear();
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
		vkSubmitInfo.signalSemaphoreCount = syncData.signalSemaphoreCount;
		vkSubmitInfo.pSignalSemaphores = syncData.pSignalSemaphores;

		VkQueue queue = m_pRenderDevice->GetQueue(queueType);
		VkResult res = vkQueueSubmit(queue, 1, &vkSubmitInfo, syncData.signalFence);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to flush command buffers! Submit call failed with error: %s", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	StagingAllocation DeviceContextVk::AllocateStaging(u64 sizeBytes, u64 alignment)
	{
		return m_stagingPool.Allocate(sizeBytes, alignment);
	}

	void DeviceContextVk::ResetStagingPool()
	{
		m_stagingPool.Reset();
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

	void DeviceContextVk::SetMetricsPointer(Metrics* pMetrics)
	{
		m_pMetrics = pMetrics;
	}

	void DeviceContextVk::ResetMetricsPointer()
	{
		m_pMetrics = nullptr;
	}

	void DeviceContextVk::SetQueryPool(VkQueryPool queryPool, u32 frameBaseQueryIndex)
	{
		m_queryPool = queryPool;
		m_queryFrameBaseIndex = frameBaseQueryIndex;
		m_beginTimestampWritten = false;
	}

	void DeviceContextVk::ResetQueryPool()
	{
		m_queryPool = VK_NULL_HANDLE;
		m_queryFrameBaseIndex = 0;
		m_beginTimestampWritten = false;
	}

	STATUS_CODE DeviceContextVk::WriteEndTimestamp()
	{
		if (m_queryPool == VK_NULL_HANDLE)
		{
			return STATUS_CODE::SUCCESS;
		}

		if (m_submissionBatches.empty())
		{
			LogError("Failed to write end timestamp. No command buffers have been recorded this frame");
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Write the end timestamp into the last command buffer. Since batches are chained
		// with semaphores, the last batch only starts after all previous ones complete.
		// BOTTOM_OF_PIPE fires after all work in that command buffer finishes, so the
		// delta between begin (first cmd buffer, TOP_OF_PIPE) and end (last cmd buffer,
		// BOTTOM_OF_PIPE) captures the entire frame's GPU execution time
		VkCommandBuffer cmdBuffer = m_submissionBatches.back().cmdBuffer;
		vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, m_queryFrameBaseIndex + 1);
		return STATUS_CODE::SUCCESS;
	}
}