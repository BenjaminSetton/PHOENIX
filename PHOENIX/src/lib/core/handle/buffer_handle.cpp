
#include "PHX/interface/buffer.h"
#include "PHX/interface/render_device.h"

#include "core/handle/handle_utils.h"

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