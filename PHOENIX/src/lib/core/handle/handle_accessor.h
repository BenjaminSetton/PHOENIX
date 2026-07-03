#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/handle_types.h"
#include "PHX/interface/handle.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;

	struct HandleAccessor
	{
		static void PopulateHandle(Handle& handle, HandleOwner* pOwner, u32 index, u8 generation);
		static HandleOwner* GetOwner(const Handle& handle);
	};
}