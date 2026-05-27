
#include "PHX/interface/ref.h"

namespace PHX
{
	RefCounted::RefCounted() : m_refCount(0)
	{
	}

	RefCounted::~RefCounted()
	{
	}

	i32 RefCounted::GetRefCount() const
	{
		return m_refCount.load();
	}

	void RefCounted::IncrementRefCount()
	{
		m_refCount.store(m_refCount.load() + 1);
	}

	void RefCounted::DecrementRefCount()
	{
		m_refCount.store(m_refCount.load() - 1);
	}
}