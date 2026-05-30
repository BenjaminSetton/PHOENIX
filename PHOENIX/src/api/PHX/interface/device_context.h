#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/pipeline.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/clear_color.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"
#include "PHX/types/vec_types.h"

#include "PHX/interface/handle.h"
#include "PHX/interface/ref.h"

namespace PHX
{
	struct DeviceContextCreateInfo
	{
		// command pool
	};

	struct DeviceContextHandle : public Handle
	{
		DeviceContextHandle();
		DeviceContextHandle(const Handle& other);
		~DeviceContextHandle();
		DeviceContextHandle(const DeviceContextHandle& other);
		DeviceContextHandle& operator=(const DeviceContextHandle& other);
		DeviceContextHandle(DeviceContextHandle&& other) noexcept;

		STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer);
		STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer);
		STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection, IPipeline* pPipeline);
		STATUS_CODE BindPipeline(IPipeline* pPipeline);
		STATUS_CODE SetViewport(Vec2u size, Vec2u offset);
		STATUS_CODE SetScissor(Vec2u size, Vec2u offset);

		STATUS_CODE Draw(u32 vertexCount);
		STATUS_CODE DrawIndexed(u32 indexCount);
		STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount);

		STATUS_CODE Dispatch(Vec3u dimensions);

		STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes);
		STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes);
	};

	class IDeviceContext : public RefCounted
	{
	public:

		virtual ~IDeviceContext() { }

		virtual STATUS_CODE BindVertexBuffer(BufferHandle vertexBuffer) = 0;
		virtual STATUS_CODE BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer) = 0;
		virtual STATUS_CODE BindUniformCollection(UniformCollectionHandle uniformCollection, IPipeline* pPipeline) = 0;
		virtual STATUS_CODE BindPipeline(IPipeline* pPipeline) = 0; // TODO - Switch over to render graph?
		virtual STATUS_CODE SetViewport(Vec2u size, Vec2u offset) = 0;
		virtual STATUS_CODE SetScissor(Vec2u size, Vec2u offset) = 0;

		virtual STATUS_CODE Draw(u32 vertexCount) = 0;
		virtual STATUS_CODE DrawIndexed(u32 indexCount) = 0;
		virtual STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount) = 0;

		virtual STATUS_CODE Dispatch(Vec3u dimensions) = 0;

		virtual STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes) = 0;
		virtual STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes) = 0;
	};
}