#include "lod_manager.h"

#include <iostream>

namespace Common
{
	LodManager::LodManager()
		: m_device{}
		, m_renderGraph{}
	{
	}

	LodManager::~LodManager()
	{
	}

	void LodManager::Initialize(PHX::RenderDeviceHandle device, PHX::RenderGraphHandle renderGraph,
		const std::vector<AssetDiskLodGroup>& lodGroups)
	{
		m_device = device;
		m_renderGraph = renderGraph;
		m_lodGroups = lodGroups;

		// Build GPU descriptors and count totals
		m_totalLodLevels = 0;
		m_totalVertexCount = 0;
		m_totalIndexCount = 0;

		m_lodGroupsGpu.reserve(lodGroups.size());
		m_lodLevelsGpu.reserve(64); // reasonable initial estimate

		uint32_t instanceListOffset = 0;

		for (uint32_t g = 0; g < lodGroups.size(); g++)
		{
			LodGroupGpu groupGpu{};
			groupGpu.levelOffset = m_totalLodLevels;
			groupGpu.levelCount = static_cast<uint32_t>(lodGroups[g].levels.size());

			for (uint32_t l = 0; l < lodGroups[g].levels.size(); l++)
			{
				const AssetDiskLodLevel& level = lodGroups[g].levels[l];

				LodLevelGpu levelGpu{};
				levelGpu.firstVertex = m_totalVertexCount;
				levelGpu.vertexCount = static_cast<uint32_t>(level.vertices.size());
				levelGpu.firstIndex = m_totalIndexCount;
				levelGpu.indexCount = static_cast<uint32_t>(level.indices.size());
				levelGpu.screenRatio = level.screenRatio;
				levelGpu.instanceListOffset = instanceListOffset;
				// Each LOD level can have up to m_instanceCount instances (worst case)
				// We'll set this after we know the instance count. For now, use a placeholder.
				instanceListOffset += 65536; // Max instances per LOD level (will be set properly later)

				m_lodLevelsGpu.push_back(levelGpu);

				m_totalVertexCount += levelGpu.vertexCount;
				m_totalIndexCount += levelGpu.indexCount;
				m_totalLodLevels++;
			}

			m_lodGroupsGpu.push_back(groupGpu);
		}

		// Allocate GPU buffers
		PHX::STATUS_CODE phxRes;

		// Vertex buffer (combined across all LOD levels of all groups)
		if (m_totalVertexCount > 0)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodVertexBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_VERTEX_BUFFER;
			ci.sizeBytes = m_totalVertexCount * sizeof(LodMeshVertex);
			phxRes = m_device.AllocateBuffer(ci, m_vertexBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate vertex buffer!" << std::endl;
				return;
			}
		}

		// Index buffer (combined)
		if (m_totalIndexCount > 0)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodIndexBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_INDEX_BUFFER;
			ci.sizeBytes = m_totalIndexCount * sizeof(AssetIndexType);
			phxRes = m_device.AllocateBuffer(ci, m_indexBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate index buffer!" << std::endl;
				return;
			}
		}

		// Instance buffer — allocated on first SetInstances call
		// (PHX doesn't support buffer resize, so we reallocate when the count grows)

		// Draw args buffer — one DrawIndexedIndirectCommand per LOD level
		if (m_totalLodLevels > 0)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodArgsBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER | PHX::BUFFER_USAGE_FLAG_INDIRECT_BUFFER;
			ci.sizeBytes = m_totalLodLevels * sizeof(DrawIndexedIndirectCommand);
			phxRes = m_device.AllocateBuffer(ci, m_argsBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate args buffer!" << std::endl;
				return;
			}
		}

		// Count buffer — single uint32 for IndirectCount
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodCountBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER | PHX::BUFFER_USAGE_FLAG_INDIRECT_BUFFER;
			ci.sizeBytes = sizeof(uint32_t);
			phxRes = m_device.AllocateBuffer(ci, m_countBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate count buffer!" << std::endl;
				return;
			}
		}

		// Instance index list buffer — compacted instance indices per LOD level
		// Each LOD level gets up to m_instanceCount slots (worst case: all instances in one LOD)
		{
			uint32_t totalIndexListSlots = m_totalLodLevels * 65536; // matches the placeholder above
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodInstanceIndexList";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER;
			ci.sizeBytes = totalIndexListSlots * sizeof(uint32_t);
			phxRes = m_device.AllocateBuffer(ci, m_instanceIndexListBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate instance index list buffer!" << std::endl;
				return;
			}
		}

		// LOD level descriptor buffer
		if (m_totalLodLevels > 0)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodLevelBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER;
			ci.sizeBytes = m_totalLodLevels * sizeof(LodLevelGpu);
			phxRes = m_device.AllocateBuffer(ci, m_lodLevelBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate LOD level buffer!" << std::endl;
				return;
			}
		}

		// LOD group descriptor buffer
		if (!m_lodGroupsGpu.empty())
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodGroupBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER;
			ci.sizeBytes = static_cast<uint64_t>(m_lodGroupsGpu.size()) * sizeof(LodGroupGpu);
			phxRes = m_device.AllocateBuffer(ci, m_lodGroupBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate LOD group buffer!" << std::endl;
				return;
			}
		}

		// Camera UBO (for compute/cull pass — uses LOD camera)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodCameraBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
			ci.sizeBytes = sizeof(LodCameraData);
			phxRes = m_device.AllocateBuffer(ci, m_cameraBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate camera buffer!" << std::endl;
				return;
			}
		}

		// Camera UBO (for graphics/render pass — uses active camera)
		{
			PHX::BufferCreateInfo ci{};
			ci.pName = "LodRenderCameraBuffer";
			ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_UNIFORM_BUFFER;
			ci.sizeBytes = sizeof(LodCameraData);
			phxRes = m_device.AllocateBuffer(ci, m_renderCameraBuffer);
			if (phxRes != PHX::STATUS_CODE::SUCCESS)
			{
				std::cout << "[LOD] Failed to allocate render camera buffer!" << std::endl;
				return;
			}
		}

		std::cout << "[LOD] Initialized with " << m_lodGroups.size() << " groups, "
			<< m_totalLodLevels << " LOD levels, "
			<< m_totalVertexCount << " vertices, "
			<< m_totalIndexCount << " indices" << std::endl;
	}

	void LodManager::SetInstances(const std::vector<LodInstanceData>& instances)
	{
		m_instances = instances;
		m_instanceCount = static_cast<uint32_t>(instances.size());

		// Reallocate instance buffer if the current one is too small.
		// PHX doesn't support buffer resize, so we drop the old handle and allocate a new one.
		uint64_t requiredSize = static_cast<uint64_t>(m_instanceCount) * sizeof(LodInstanceData);
		if (m_instanceBufferSize < requiredSize)
		{
			m_instanceBuffer = PHX::BufferHandle{};
			if (m_instanceCount > 0)
			{
				PHX::BufferCreateInfo ci{};
				ci.pName = "LodInstanceBuffer";
				ci.bufferUsage = PHX::BUFFER_USAGE_FLAG_STORAGE_BUFFER;
				ci.sizeBytes = requiredSize;
				PHX::STATUS_CODE phxRes = m_device.AllocateBuffer(ci, m_instanceBuffer);
				if (phxRes != PHX::STATUS_CODE::SUCCESS)
				{
					std::cout << "[LOD] Failed to reallocate instance buffer!" << std::endl;
					m_instanceCount = 0;
					return;
				}
				m_instanceBufferSize = requiredSize;
			}
		}
	}

	void LodManager::UploadStaticData(PHX::RenderGraphHandle renderGraph)
	{
		if (m_staticDataUploaded) return;
		m_staticDataUploaded = true;

		PHX::RenderPassHandle transferPass;
		PHX::STATUS_CODE phxRes = renderGraph.RegisterPass("LodDataUpload", PHX::PASS_TYPE::TRANSFER, transferPass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		if (m_vertexBuffer.IsValid()) transferPass.SetBufferOutput(m_vertexBuffer);
		if (m_indexBuffer.IsValid()) transferPass.SetBufferOutput(m_indexBuffer);
		if (m_lodLevelBuffer.IsValid()) transferPass.SetBufferOutput(m_lodLevelBuffer);
		if (m_lodGroupBuffer.IsValid()) transferPass.SetBufferOutput(m_lodGroupBuffer);

		// Build combined vertex and index arrays
		// Repack AssetDiskVertex (88 bytes) into compact LodMeshVertex (32 bytes) to match
		// the pipeline's auto-calculated vertex stride (position + normal + uv).
		std::vector<LodMeshVertex> allVertices;
		std::vector<AssetIndexType> allIndices;
		allVertices.reserve(m_totalVertexCount);
		allIndices.reserve(m_totalIndexCount);

		for (const auto& group : m_lodGroups)
		{
			for (const auto& level : group.levels)
			{
				for (const auto& v : level.vertices)
				{
					LodMeshVertex compact{};
					compact.position = v.position;
					compact.normal = v.normal;
					compact.uv = v.uv;
					allVertices.push_back(compact);
				}
				allIndices.insert(allIndices.end(), level.indices.begin(), level.indices.end());
			}
		}

		transferPass.SetExecuteCallback([this, allVertices, allIndices](PHX::DeviceContextHandle deviceContext) mutable
		{
			if (m_vertexBuffer.IsValid() && !allVertices.empty())
			{
				deviceContext.CopyDataToBuffer(m_vertexBuffer, allVertices.data(), allVertices.size() * sizeof(LodMeshVertex));
			}
			if (m_indexBuffer.IsValid() && !allIndices.empty())
			{
				deviceContext.CopyDataToBuffer(m_indexBuffer, allIndices.data(), allIndices.size() * sizeof(AssetIndexType));
			}
			if (m_lodLevelBuffer.IsValid() && !m_lodLevelsGpu.empty())
			{
				deviceContext.CopyDataToBuffer(m_lodLevelBuffer, m_lodLevelsGpu.data(), m_lodLevelsGpu.size() * sizeof(LodLevelGpu));
			}
			if (m_lodGroupBuffer.IsValid() && !m_lodGroupsGpu.empty())
			{
				deviceContext.CopyDataToBuffer(m_lodGroupBuffer, m_lodGroupsGpu.data(), m_lodGroupsGpu.size() * sizeof(LodGroupGpu));
			}
		});
	}

	void LodManager::UploadInstances(PHX::RenderGraphHandle renderGraph)
	{
		if (m_instanceCount == 0) return;

		PHX::RenderPassHandle transferPass;
		PHX::STATUS_CODE phxRes = renderGraph.RegisterPass("LodInstanceUpload", PHX::PASS_TYPE::TRANSFER, transferPass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		transferPass.SetBufferOutput(m_instanceBuffer);

		transferPass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
		{
			deviceContext.CopyDataToBuffer(m_instanceBuffer, m_instances.data(), m_instanceCount * sizeof(LodInstanceData));
		});
	}

	void LodManager::ZeroDrawBuffers(PHX::RenderGraphHandle renderGraph)
	{
		// Zero the args and count buffers in a transfer pass. Must be called before the
		// compute cull pass. The compute shader uses InterlockedAdd on instanceCount,
		// so it must start at 0.
		PHX::RenderPassHandle transferPass;
		PHX::STATUS_CODE phxRes = renderGraph.RegisterPass("LodZeroDrawBuffers", PHX::PASS_TYPE::TRANSFER, transferPass);
		if (phxRes != PHX::STATUS_CODE::SUCCESS) return;

		transferPass.SetBufferOutput(m_argsBuffer);
		transferPass.SetBufferOutput(m_countBuffer);

		transferPass.SetExecuteCallback([this](PHX::DeviceContextHandle deviceContext)
		{
			std::vector<uint8_t> zeros(m_totalLodLevels * sizeof(DrawIndexedIndirectCommand), 0);
			deviceContext.CopyDataToBuffer(m_argsBuffer, zeros.data(), zeros.size());
			uint32_t zeroCount = 0;
			deviceContext.CopyDataToBuffer(m_countBuffer, &zeroCount, sizeof(zeroCount));
		});
	}

	void LodManager::CullAndSelectGPU(PHX::DeviceContextHandle deviceContext, PHX::UniformCollectionHandle computeUniforms, const LodCameraData& cameraData)
	{
		// Update camera UBO (uniform buffer uses direct memory mapping, no command buffer switch)
		deviceContext.CopyDataToBuffer(m_cameraBuffer, &cameraData, sizeof(LodCameraData));

		// Args and count buffers are zeroed in ZeroDrawBuffers() which should be called
		// in a separate transfer pass before this compute pass. Calling CopyDataToBuffer
		// here would switch to the transfer queue's command buffer, causing the compute
		// pipeline binding to be lost.

		// Queue all buffer bindings for the compute shader
		// Binding layout (set 0):
		//   0: InstanceBuffer (SSBO)
		//   1: LodLevelBuffer (SSBO)
		//   2: LodGroupBuffer (SSBO)
		//   3: ArgsBuffer (RWSSBO)
		//   4: CountBuffer (RWSSBO)
		//   5: CameraBuffer (UBO)
		//   6: InstanceIndexListBuffer (RWSSBO)
		computeUniforms.QueueBufferUpdate(m_instanceBuffer, 0, 0, 0);
		computeUniforms.QueueBufferUpdate(m_lodLevelBuffer, 0, 1, 0);
		computeUniforms.QueueBufferUpdate(m_lodGroupBuffer, 0, 2, 0);
		computeUniforms.QueueBufferUpdate(m_argsBuffer, 0, 3, 0);
		computeUniforms.QueueBufferUpdate(m_countBuffer, 0, 4, 0);
		computeUniforms.QueueBufferUpdate(m_cameraBuffer, 0, 5, 0);
		computeUniforms.QueueBufferUpdate(m_instanceIndexListBuffer, 0, 6, 0);

		deviceContext.FlushUniformUpdates(computeUniforms);
		deviceContext.BindUniformCollection(computeUniforms);

		// Dispatch one thread per instance
		uint32_t threadCount = m_instanceCount;
		// Round up to multiple of 64 (workgroup size)
		uint32_t workgroups = (threadCount + 63) / 64;
		deviceContext.Dispatch(PHX::Vec3u(workgroups * 64, 1, 1));
	}

	void LodManager::DrawIndirect(PHX::DeviceContextHandle deviceContext, bool useIndirectCount)
	{
		if (!m_vertexBuffer.IsValid() || !m_indexBuffer.IsValid() || !m_argsBuffer.IsValid())
			return;

		// Bind the combined vertex/index buffer
		deviceContext.BindMesh(m_vertexBuffer, m_indexBuffer, PHX::INDEX_TYPE::U32);

		if (useIndirectCount && m_countBuffer.IsValid())
		{
			// GPU-driven: count is in the count buffer, written by the compute shader
			deviceContext.DrawIndexedIndirectCount(
				m_argsBuffer, 0,
				m_countBuffer, 0,
				m_totalLodLevels,
				sizeof(DrawIndexedIndirectCommand));
		}
		else
		{
			// Fallback: fixed draw count = total LOD levels. Compute shader zeroes
			// instanceCount for LOD levels with no visible instances.
			deviceContext.DrawIndexedIndirect(
				m_argsBuffer,
				m_totalLodLevels,
				sizeof(DrawIndexedIndirectCommand),
				0);
		}
	}
}
