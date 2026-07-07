
#include "PHX/interface/buffer.h"
#include "PHX/interface/render_device.h"

#include "core/handle/handle_utils.h"
#include "core/interface_types/buffer_interface.h"

namespace PHX
{
	DEFINE_PHX_HANDLE(BufferHandle, HANDLE_TYPE::BUFFER)

	const char* BufferHandle::GetName() const
	{
		IBuffer* pBuffer = HANDLE_UTILS::ResolveHandle(*this);
		if (pBuffer != nullptr)
		{
			return pBuffer->GetName();
		}

		return nullptr;
	}

	BUFFER_USAGE BufferHandle::GetUsage() const
	{
		IBuffer* pBuffer = HANDLE_UTILS::ResolveHandle(*this);
		if (pBuffer != nullptr)
		{
			return pBuffer->GetUsage();
		}

		// No other sensible default
		return BUFFER_USAGE::UNIFORM_BUFFER;
	}

	u64 BufferHandle::GetSize() const
	{
		IBuffer* pBuffer = HANDLE_UTILS::ResolveHandle(*this);
		if (pBuffer != nullptr)
		{
			return pBuffer->GetSize();
		}

		return 0;
	}
}