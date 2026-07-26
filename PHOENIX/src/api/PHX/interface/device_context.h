#pragma once

#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/clear_color.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"
#include "PHX/types/vec_types.h"

#include "PHX/interface/handle.h"

namespace PHX
{
	struct DeviceContextCreateInfo
	{
		u32 assignedFrameIndex;
	};

	struct PHX_API DeviceContextHandle : public Handle
	{
		DECLARE_PHX_HANDLE(DeviceContextHandle);

		STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer);
		STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer, INDEX_TYPE indexType = INDEX_TYPE::U32);
		STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection);
		STATUS_CODE FlushUniformUpdates(UniformCollectionHandle uniformCollection);
		STATUS_CODE SetViewport(Vec2u size, Vec2u offset);
		STATUS_CODE SetScissor(Vec2u size, Vec2u offset);

		STATUS_CODE Draw(u32 vertexCount);
		STATUS_CODE DrawIndexed(u32 indexCount, u32 firstIndex = 0, u32 vertexOffset = 0);
		STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex = 0, u32 vertexOffset = 0, u32 instanceOffset = 0);

		STATUS_CODE Dispatch(Vec3u dimensions);
		STATUS_CODE TraceRays(Vec3u dimensions);

		STATUS_CODE BuildBottomLevelAccelerationStructure(AccelerationStructureHandle handle);
		STATUS_CODE BuildTopLevelAccelerationStructure(AccelerationStructureHandle handle, BufferHandle instanceBuffer, u32 instanceCount);

		STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes);
		STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes, u32 mipLevel = 0);
	};
}