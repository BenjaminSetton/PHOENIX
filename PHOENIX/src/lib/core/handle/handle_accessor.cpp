
#include "handle_accessor.h"

namespace PHX
{
	void HandleAccessor::PopulateHandle(Handle& handle, HandleOwner* pOwner, u32 index)
	{
		handle.PopulateHandle(pOwner, index);
	}

	HandleOwner* HandleAccessor::GetOwner(const Handle& handle)
	{
		return handle.m_pOwner;
	}
}