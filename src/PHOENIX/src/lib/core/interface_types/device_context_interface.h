#pragma once

#include "core/ref.h"
#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/metrics.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	class IDeviceContext : public RefCounted
	{
	public:

		virtual ~IDeviceContext() { }

		virtual STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer) = 0;
		virtual STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer, INDEX_TYPE indexType) = 0;
		virtual STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection) = 0;
		virtual STATUS_CODE FlushUniformUpdates(UniformCollectionHandle uniformCollection) = 0;
		virtual STATUS_CODE SetViewport(BSL::Vec2u size, BSL::Vec2u offset) = 0;
		virtual STATUS_CODE SetScissor(BSL::Vec2u size, BSL::Vec2u offset) = 0;

		virtual STATUS_CODE Draw(u32 vertexCount) = 0;
		virtual STATUS_CODE DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset) = 0;
		virtual STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 instanceOffset) = 0;
		virtual STATUS_CODE DrawIndexedIndirect(BufferHandle argsBuffer, u32 drawCount, u32 stride, u64 argsOffset) = 0;
		virtual STATUS_CODE DrawIndexedIndirectCount(BufferHandle argsBuffer, u64 argsOffset, BufferHandle countBuffer, u64 countOffset, u32 maxDrawCount, u32 stride) = 0;

		virtual STATUS_CODE Dispatch(BSL::Vec3u dimensions) = 0;
		virtual STATUS_CODE TraceRays(BSL::Vec3u dimensions) = 0;

		virtual STATUS_CODE BuildBottomLevelAccelerationStructure(AccelerationStructureHandle handle) = 0;
		virtual STATUS_CODE BuildTopLevelAccelerationStructure(AccelerationStructureHandle handle, BufferHandle instanceBuffer, u32 instanceCount) = 0;

		virtual STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes) = 0;
		virtual STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes, u32 mipLevel) = 0;

		virtual void SetMetricsPointer(Metrics* pMetrics) = 0;
		virtual void ResetMetricsPointer() = 0;
	};
}