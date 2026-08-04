#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/handle_types.h"
#include "PHX/interface/handle.h"

namespace PHX
{
	struct HandleAccessor
	{
		static void PopulateHandle(Handle& handle, HandleOwner* pOwner, u32 index);
		static HandleOwner* GetOwner(const Handle& handle);
	};
}