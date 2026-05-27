
#include "PHX/interface/buffer.h"
#include "PHX/interface/render_device.h"

namespace PHX
{
	BufferHandle::BufferHandle() : Handle(HANDLE_TYPE::BUFFER)
	{
	}

	BufferHandle::BufferHandle(const Handle& base) : Handle(base)
	{
	}

	BufferHandle::~BufferHandle()
	{
	}

	BufferHandle::BufferHandle(const BufferHandle& other) : Handle(other)
	{
	}

	BufferHandle& BufferHandle::operator=(const BufferHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	BufferHandle::BufferHandle(BufferHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	BUFFER_USAGE BufferHandle::GetUsage() const
	{
		IBuffer* pBuffer = m_pRenderDevice->ResolveHandle(*this);
		if (pBuffer != nullptr)
		{
			return pBuffer->GetUsage();
		}

		// No other sensible default
		return BUFFER_USAGE::UNIFORM_BUFFER;
	}

	u64 BufferHandle::GetSize() const
	{
		IBuffer* pBuffer = m_pRenderDevice->ResolveHandle(*this);
		if (pBuffer != nullptr)
		{
			return pBuffer->GetSize();
		}

		return 0;
	}
}