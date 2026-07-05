#pragma once

#include "PHX/interface/ref.h"
#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
	class IBuffer : public RefCounted
	{
	public:

		virtual ~IBuffer() { }

		virtual const char* GetName() const = 0;
		virtual BUFFER_USAGE GetUsage() const = 0;
		virtual u64 GetSize() const = 0;
	};
}