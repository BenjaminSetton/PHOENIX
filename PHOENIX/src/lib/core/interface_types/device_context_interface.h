#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/ref.h"
#include "PHX/interface/uniform.h"
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
		virtual STATUS_CODE SetViewport(Vec2u size, Vec2u offset) = 0;
		virtual STATUS_CODE SetScissor(Vec2u size, Vec2u offset) = 0;

		virtual STATUS_CODE Draw(u32 vertexCount) = 0;
		virtual STATUS_CODE DrawIndexed(u32 indexCount, u32 firstIndex, u32 vertexOffset) = 0;
		virtual STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 instanceOffset) = 0;

		virtual STATUS_CODE Dispatch(Vec3u dimensions) = 0;

		virtual STATUS_CODE CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes) = 0;
		virtual STATUS_CODE CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes) = 0;
	};
}