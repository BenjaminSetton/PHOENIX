
#include "PHX/interface/device_context.h"
#include "PHX/interface/render_device.h"

#include "core/handle/handle_utils.h"
#include "utils/logger.h"

namespace PHX
{
	DeviceContextHandle::DeviceContextHandle() : Handle(HANDLE_TYPE::DEVICE_CONTEXT)
	{
	}

	DeviceContextHandle::DeviceContextHandle(const Handle& other) : Handle(other)
	{
	}

	DeviceContextHandle::~DeviceContextHandle()
	{
	}

	DeviceContextHandle::DeviceContextHandle(const DeviceContextHandle& other) : Handle(other)
	{
	}

	DeviceContextHandle& DeviceContextHandle::operator=(const DeviceContextHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	DeviceContextHandle::DeviceContextHandle(DeviceContextHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	STATUS_CODE DeviceContextHandle::BindVertexBuffer(BufferHandle vertexBuffer)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->BindVertexBuffer(vertexBuffer);
		}

		LogError("Failed to bind vertex buffer. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::BindMesh(BufferHandle vertexBuffer, BufferHandle indexBuffer)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->BindMesh(vertexBuffer, indexBuffer);
		}

		LogError("Failed to bind mesh. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::BindUniformCollection(UniformCollectionHandle uniformCollection)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->BindUniformCollection(uniformCollection);
		}

		LogError("Failed to bind uniform collection. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::SetViewport(Vec2u size, Vec2u offset)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->SetViewport(size, offset);
		}

		LogError("Failed to set viewport. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::SetScissor(Vec2u size, Vec2u offset)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->SetScissor(size, offset);
		}

		LogError("Failed to set scissor. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::Draw(u32 vertexCount)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->Draw(vertexCount);
		}

		LogError("Failed to issue draw call. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::DrawIndexed(u32 indexCount)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->DrawIndexed(indexCount);
		}

		LogError("Failed to issue draw indexed call. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::DrawIndexedInstanced(u32 indexCount, u32 instanceCount)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->DrawIndexedInstanced(indexCount, instanceCount);
		}

		LogError("Failed to issue draw indexed instanced call. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::Dispatch(Vec3u dimensions)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->Dispatch(dimensions);
		}

		LogError("Failed to issue dispatch call. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::CopyDataToBuffer(BufferHandle buffer, const void* data, u64 sizeBytes)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->CopyDataToBuffer(buffer, data, sizeBytes);
		}

		LogError("Failed to copy data to buffer. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE DeviceContextHandle::CopyDataToTexture(TextureHandle texture, const void* data, u64 sizeBytes)
	{
		IDeviceContext* pContext = HANDLE_UTILS::ResolveHandle(*this);
		if (pContext != nullptr)
		{
			return pContext->CopyDataToTexture(texture, data, sizeBytes);
		}

		LogError("Failed to copy data to texture. Could not resolve device context handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}
}
