#pragma once

#include <BSL/vec_types.h>
#include <PHX/phx.h>
#include <glm.hpp>

#include <vector>
#include <cstdint>

#include "utils/asset_importer.h"

namespace Common
{
	// GPU-side instance data: model matrix + LOD group index
	struct LodInstanceData
	{
		glm::mat4 modelMatrix;  // Transposed for row-major GPU convention
		uint32_t  groupIndex;
		uint32_t  _pad[3];
	};

	// Per-LOD-level GPU descriptor: vertex/index offsets into the combined buffers
	struct LodLevelGpu
	{
		uint32_t firstVertex;
		uint32_t vertexCount;
		uint32_t firstIndex;
		uint32_t indexCount;
		float    screenRatio;
		uint32_t instanceListOffset;  // Offset into the global instance index list
		uint32_t _pad[2];
	};

	// Per-LOD-group GPU descriptor
	struct LodGroupGpu
	{
		uint32_t levelOffset;   // Offset into the LodLevelGpu array
		uint32_t levelCount;
		uint32_t _pad[2];
	};

	// Compact vertex format matching the mesh vertex shader's input attributes:
	// location 0: position (vec3), location 1: normal (vec3), location 2: uv (vec2)
	struct LodMeshVertex
	{
		BSL::Vec3f position;
		BSL::Vec3f normal;
		BSL::Vec2f uv;
	};

	// Camera data for the cull shader
	struct LodCameraData
	{
		glm::mat4 viewMatrix;
		glm::mat4 projMatrix;
		glm::vec4 cameraPos;     // xyz = position, w = unused
		glm::vec4 screenParams;  // x = screenWidth, y = screenHeight, z = fov, w = aspect
		uint32_t  instanceCount;
		uint32_t  groupCount;
		uint32_t  _pad[2];
	};

	// VkDrawIndexedIndirectCommand layout (matches Vulkan spec)
	struct DrawIndexedIndirectCommand
	{
		uint32_t indexCount;
		uint32_t instanceCount;
		uint32_t firstIndex;
		int32_t  vertexOffset;
		uint32_t firstInstance;
	};

	// Manages LOD groups, instance data, GPU buffers, and the compute cull pass.
	// The sample creates instances, calls CullAndSelectGPU() each frame inside a
	// compute render graph pass, then issues indirect draws in a graphics pass.
	class LodManager
	{
	public:
		LodManager();
		~LodManager();

		LodManager(const LodManager&) = delete;
		LodManager& operator=(const LodManager&) = delete;

		// Initialize from imported LOD groups. Allocates all GPU buffers.
		// Each LOD group's levels are concatenated into shared vertex/index buffers.
		void Initialize(PHX::RenderDeviceHandle device, PHX::RenderGraphHandle renderGraph,
			const std::vector<AssetDiskLodGroup>& lodGroups);

		// Set instance data (CPU-side). Call before UploadInstances().
		void SetInstances(const std::vector<LodInstanceData>& instances);

		// Upload static data (vertex/index/lod descriptors) to GPU. Call once during init.
		void UploadStaticData(PHX::RenderGraphHandle renderGraph);

		// Upload instance data to GPU. Call when instance data changes.
		void UploadInstances(PHX::RenderGraphHandle renderGraph);

		// Zero the args and count buffers. Must be called before CullAndSelectGPU each frame.
		void ZeroDrawBuffers(PHX::RenderGraphHandle renderGraph);

		// GPU cull + LOD select. Call inside a compute render graph pass callback.
		// Writes draw commands into the args buffer.
		void CullAndSelectGPU(PHX::DeviceContextHandle deviceContext, PHX::UniformCollectionHandle computeUniforms, const LodCameraData& cameraData);

		// Issue indirect draws. Call inside a graphics render graph pass callback.
		void DrawIndirect(PHX::DeviceContextHandle deviceContext, bool useIndirectCount);

		// Getters for render graph resource tracking
		PHX::BufferHandle GetVertexBuffer() const { return m_vertexBuffer; }
		PHX::BufferHandle GetIndexBuffer() const { return m_indexBuffer; }
		PHX::BufferHandle GetInstanceBuffer() const { return m_instanceBuffer; }
		PHX::BufferHandle GetArgsBuffer() const { return m_argsBuffer; }
		PHX::BufferHandle GetCountBuffer() const { return m_countBuffer; }
		PHX::BufferHandle GetLodLevelBuffer() const { return m_lodLevelBuffer; }
		PHX::BufferHandle GetLodGroupBuffer() const { return m_lodGroupBuffer; }
		PHX::BufferHandle GetCameraBuffer() const { return m_cameraBuffer; }
		PHX::BufferHandle GetRenderCameraBuffer() const { return m_renderCameraBuffer; }
		PHX::BufferHandle GetInstanceIndexListBuffer() const { return m_instanceIndexListBuffer; }

		uint32_t GetInstanceCount() const { return m_instanceCount; }
		uint32_t GetGroupCount() const { return static_cast<uint32_t>(m_lodGroups.size()); }
		uint32_t GetTotalLodLevels() const { return m_totalLodLevels; }

		// Max draws = total LOD levels (one draw per LOD level per group, worst case)
		uint32_t GetMaxDrawCount() const { return m_totalLodLevels; }

	private:
		PHX::RenderDeviceHandle m_device;
		PHX::RenderGraphHandle  m_renderGraph;

		// GPU buffers
		PHX::BufferHandle m_vertexBuffer;
		PHX::BufferHandle m_indexBuffer;
		PHX::BufferHandle m_instanceBuffer;
		PHX::BufferHandle m_argsBuffer;      // DrawIndexedIndirectCommand per LOD level
		PHX::BufferHandle m_countBuffer;     // uint32_t draw count for IndirectCount
		PHX::BufferHandle m_lodLevelBuffer;  // LodLevelGpu array
		PHX::BufferHandle m_lodGroupBuffer;  // LodGroupGpu array
		PHX::BufferHandle m_cameraBuffer;    // LodCameraData UBO (for compute/cull pass)
		PHX::BufferHandle m_renderCameraBuffer; // LodCameraData UBO (for graphics/render pass)
		PHX::BufferHandle m_instanceIndexListBuffer; // Compacted instance indices per LOD level

		// CPU-side data
		std::vector<AssetDiskLodGroup> m_lodGroups;
		std::vector<LodInstanceData>   m_instances;
		std::vector<LodLevelGpu>       m_lodLevelsGpu;
		std::vector<LodGroupGpu>       m_lodGroupsGpu;

		uint32_t m_instanceCount = 0;
		uint64_t m_instanceBufferSize = 0;  // Tracks allocated size of m_instanceBuffer
		uint32_t m_totalLodLevels = 0;
		uint32_t m_totalVertexCount = 0;
		uint32_t m_totalIndexCount = 0;

		bool m_staticDataUploaded = false;
	};
}
