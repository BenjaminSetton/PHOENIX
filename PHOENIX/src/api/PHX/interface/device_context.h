#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/framebuffer.h"
#include "PHX/interface/pipeline.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/clear_color.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"
#include "PHX/types/vec_types.h"

namespace PHX
{
	struct DeviceContextCreateInfo
	{
		// command pool
	};

	class IDeviceContext
	{
	public:

		virtual ~IDeviceContext() { }

		virtual STATUS_CODE BindVertexBuffer(IBuffer* pVertexBuffer) = 0;
		virtual STATUS_CODE BindMesh(IBuffer* pVertexBuffer, IBuffer* pIndexBuffer) = 0;
		virtual STATUS_CODE BindUniformCollection(IUniformCollection* pUniformCollection, IPipeline* pPipeline) = 0;
		virtual STATUS_CODE BindPipeline(IPipeline* pPipeline) = 0;
		virtual STATUS_CODE SetViewport(Vec2u size, Vec2u offset) = 0;
		virtual STATUS_CODE SetScissor(Vec2u size, Vec2u offset) = 0;

		virtual STATUS_CODE Draw(u32 vertexCount) = 0;
		virtual STATUS_CODE DrawIndexed(u32 indexCount) = 0;
		virtual STATUS_CODE DrawIndexedInstanced(u32 indexCount, u32 instanceCount) = 0;

		virtual STATUS_CODE Dispatch(Vec3u dimensions) = 0;

		virtual STATUS_CODE CopyDataToBuffer(IBuffer* pBuffer, const void* data, u64 sizeBytes) = 0;
	};
}