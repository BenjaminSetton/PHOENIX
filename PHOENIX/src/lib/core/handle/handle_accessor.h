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
		static void PopulateHandle(Handle& handle, IRenderDevice* pDevice, u32 index, u8 generation);
		static u32 GetIndex(const Handle& handle);
		static u32 GetGeneration(const Handle& handle);
		static HANDLE_TYPE GetType(const Handle& handle);
	};
}