#pragma once

#include "PHX/interface/handle.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// TODO - Move to lib
	class HandleOwner
	{
	public:

		virtual void* ResolveHandle(const Handle& handle)          = 0;
		virtual void IncrementHandleRefCount(const Handle& handle) = 0;
		virtual void DecrementHandleRefCount(const Handle& handle) = 0;
	};
}