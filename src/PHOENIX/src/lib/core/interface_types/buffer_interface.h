#pragma once

#include "BSL/integral_types.h"
#include "core/ref.h"
#include "PHX/types/buffer_desc.h"

namespace PHX
{
	class IBuffer : public RefCounted
	{
	public:

		virtual ~IBuffer() { }

		virtual const char* GetName() const = 0;
		virtual BufferUsageFlags GetUsage() const = 0;
		virtual u64 GetSize() const = 0;
	};
}