
#include "core/handle/handle_utils.h"
#include "core/interface_types/uniform_interface.h"
#include "PHX/interface/render_device.h"
#include "utils/logger.h"

namespace PHX
{
	DEFINE_PHX_HANDLE(UniformCollectionHandle, HANDLE_TYPE::UNIFORM)

	u32 UniformCollectionHandle::GetGroupCount() const
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroupCount();
		}

		return 0;
	}

	const UniformDataGroup* UniformCollectionHandle::GetGroup(u32 groupIndex) const
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroup(groupIndex);
		}

		return nullptr;
	}

	UniformDataGroup* UniformCollectionHandle::GetGroup(u32 groupIndex)
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->GetGroup(groupIndex);
		}

		return nullptr;
	}

	STATUS_CODE UniformCollectionHandle::QueueBufferUpdate(BufferHandle buffer, u32 set, u32 binding, u64 offset, u64 size)
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->QueueBufferUpdate(buffer, set, binding, offset, size);
		}

		LogError("Failed to queue buffer update. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE UniformCollectionHandle::QueueImageUpdate(TextureHandle texture, u32 set, u32 binding, u32 imageViewIndex)
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->QueueImageUpdate(texture, set, binding, imageViewIndex);
		}

		LogError("Failed to queue image update. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE UniformCollectionHandle::QueueAccelerationStructureUpdate(AccelerationStructureHandle accelerationStructure, u32 set, u32 binding)
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->QueueAccelerationStructureUpdate(accelerationStructure, set, binding);
		}

		LogError("Failed to queue acceleration structure update. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE UniformCollectionHandle::FlushUpdateQueue()
	{
		IUniformCollection* pUniformCollection = HANDLE_UTILS::ResolveHandle(*this);
		if (pUniformCollection != nullptr)
		{
			return pUniformCollection->FlushUpdateQueue();
		}
		
		LogError("Failed to flush update queue. Could not resolve uniform collection handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}
}