
#include "PHX/interface/ref.h"

namespace PHX
{
	RefCounted::RefCounted() : m_refCount(0)
	{
	}

	RefCounted::~RefCounted()
	{
	}

	RefCounted::RefCounted(const RefCounted& other)
	{
		m_refCount.store(other.m_refCount.load());
	}

	RefCounted& RefCounted::operator=(const RefCounted& other)
	{
		if (this == &other)
		{
			return *this;
		}

		m_refCount.store(other.m_refCount.load());
		return *this;
	}

	RefCounted::RefCounted(RefCounted&& other) noexcept
	{
		m_refCount.store(other.m_refCount.load());
		other.m_refCount.store(0);
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