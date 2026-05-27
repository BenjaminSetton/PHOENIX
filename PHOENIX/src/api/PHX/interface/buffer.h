#pragma once

#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"

#include "PHX/interface/handle.h"

#include "PHX/interface/ref.h" // TODO - Move to lib

namespace PHX
{
	struct BufferCreateInfo
	{
		u64 sizeBytes;
		BUFFER_USAGE bufferUsage;
	};

	struct BufferHandle : public Handle
	{
		BufferHandle();
		BufferHandle(const Handle& base); // Needed for down-casting from Handle?
		~BufferHandle();
		BufferHandle(const BufferHandle& other);
		BufferHandle& operator=(const BufferHandle& other);
		BufferHandle(BufferHandle&& other) noexcept;

		BUFFER_USAGE GetUsage() const;
		u64 GetSize() const;
	};

	// TODO - Move to lib
	class IBuffer : public RefCounted
	{
	public:

		virtual ~IBuffer() { }

		virtual BUFFER_USAGE GetUsage() const = 0;
		virtual u64 GetSize() const = 0;
	};
}