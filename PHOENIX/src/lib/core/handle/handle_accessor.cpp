
#include "handle_accessor.h"

namespace PHX
{
	void HandleAccessor::PopulateHandle(Handle& handle, HandleOwner* pOwner, u32 index, u8 generation)
	{
		handle.PopulateHandle(pOwner, index, generation);
	}

	HandleOwner* HandleAccessor::GetOwner(const Handle& handle)
	{
		return handle.m_pOwner;
	}
}