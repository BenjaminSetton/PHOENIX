
#include "handle_accessor.h"

namespace PHX
{
	void HandleAccessor::PopulateHandle(Handle& handle, IRenderDevice* pDevice, u32 index, u8 generation)
	{
		handle.m_pRenderDevice = pDevice;
		handle.m_index = index;
		handle.m_generation = generation;
	}

	u32 HandleAccessor::GetIndex(const Handle& handle)
	{
		return handle.m_index;
	}

	u32 HandleAccessor::GetGeneration(const Handle& handle)
	{
		return handle.m_generation;
	}

	HANDLE_TYPE HandleAccessor::GetType(const Handle& handle)
	{
		return handle.m_type;
	}
}