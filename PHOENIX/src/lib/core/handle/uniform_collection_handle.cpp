
#include "PHX/interface/uniform.h"
#include "PHX/interface/render_device.h"

#include "utils/logger.h"

namespace PHX
{
	UniformCollectionHandle::UniformCollectionHandle() : Handle(HANDLE_TYPE::UNIFORM)
	{
	}

	UniformCollectionHandle::UniformCollectionHandle(const Handle& other) : Handle(other)
	{
	}

	UniformCollectionHandle::~UniformCollectionHandle()
	{
	}

	UniformCollectionHandle::UniformCollectionHandle(const UniformCollectionHandle& other) : Handle(other)
	{
	}

	UniformCollectionHandle& UniformCollectionHandle::operator=(const UniformCollectionHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	UniformCollectionHandle::UniformCollectionHandle(UniformCollectionHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	u32 UniformCollectionHandle::GetGroupCount() const
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroupCount();
		}

		return 0;
	}

	const UniformDataGroup* UniformCollectionHandle::GetGroup(u32 groupIndex) const
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroup(groupIndex);
		}

		return nullptr;
	}

	UniformDataGroup* UniformCollectionHandle::GetGroup(u32 groupIndex)
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroup(groupIndex);
		}

		return nullptr;
	}

	STATUS_CODE UniformCollectionHandle::QueueBufferUpdate(BufferHandle buffer, u32 set, u32 binding, u64 offset, u64 size)
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->QueueBufferUpdate(buffer, set, binding, offset, size);
		}

		LogError("Failed to queue buffer update. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE UniformCollectionHandle::QueueImageUpdate(TextureHandle texture, u32 set, u32 binding, u32 imageViewIndex)
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->QueueImageUpdate(texture, set, binding, imageViewIndex);
		}

		LogError("Failed to queue image update. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE UniformCollectionHandle::FlushUpdateQueue()
	{
		IUniformCollection* pUniformCollection = m_pRenderDevice->ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->FlushUpdateQueue();
		}
		
		LogError("Failed to flush update queue. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}
}