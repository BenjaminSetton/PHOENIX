
#include "core/handle/handle_owner.h"
#include "PHX/interface/handle.h"
#include "PHX/interface/render_device.h"
#include "PHX/types/integral_types.h"
#include "utils/sanity.h"

namespace PHX
{
	static constexpr u32 INVALID_INDEX = U32_MAX;

	Handle::Handle() : m_pOwner(nullptr), m_index(INVALID_INDEX), m_type(HANDLE_TYPE::INVALID)
	{
	}

	Handle::Handle(HANDLE_TYPE type) : m_pOwner(nullptr), m_index(INVALID_INDEX), m_type(type)
	{
	}

	Handle::~Handle()
	{
		DecrementRefCount();
		Reset();
	}

	Handle::Handle(const Handle& other)
	{
		m_pOwner = other.m_pOwner;
		m_index = other.m_index;
		m_type = other.m_type;

		if (!IsSame(other, INVALID_HANDLE))
		{
			IncrementRefCount();
		}

	}

	Handle& Handle::operator=(const Handle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		// Special case - setting handle to INVALID_HANDLE will effectively
		// leak the handle by preventing it from being released in the destructor,
		// since the render device will be null
		if (IsSame(other, INVALID_HANDLE))
		{
			DecrementRefCount();
			Reset();
			return *this;
		}

		m_pOwner = other.m_pOwner;
		m_index = other.m_index;
		m_type = other.m_type;

		IncrementRefCount();

		return *this;
	}

	Handle::Handle(Handle&& other) noexcept
	{
		m_pOwner = other.m_pOwner;
		m_index = other.m_index;
		m_type = other.m_type;

		// No change to ref count
		other.Reset();
	}

	bool Handle::operator==(const Handle& other) const
	{
		return IsSame(*this, other);
	}

	bool Handle::operator!=(const Handle& other) const
	{
		return !(Handle::operator==(other));
	}

	bool Handle::IsValid() const
	{
		return (m_pOwner != nullptr && m_index != INVALID_INDEX && m_type != HANDLE_TYPE::INVALID);
	}

	bool Handle::IsEmpty() const
	{
		return (m_pOwner == nullptr && m_index == INVALID_INDEX && m_type != HANDLE_TYPE::INVALID);
	}

	u32 Handle::GetIndex() const
	{
		return m_index;
	}

	HANDLE_TYPE Handle::GetType() const
	{
		return m_type;
	}

	void Handle::Reset()
	{
		m_pOwner = nullptr;
		m_index = INVALID_INDEX;
		m_type = HANDLE_TYPE::INVALID;
	}

	void Handle::PopulateHandle(HandleOwner* pOwner, u32 index)
	{
		m_pOwner = pOwner;
		m_index = index;
		// Do not change the type!

		// HandleAccessor calls this when a new handle is created
		IncrementRefCount();
	}

	bool Handle::IsSame(const Handle& handleA, const Handle& handleB) const
	{
		return (memcmp(&handleA, &handleB, sizeof(Handle)) == 0);
	}

	void Handle::IncrementRefCount()
	{
		if (m_pOwner != nullptr)
		{
			m_pOwner->IncrementHandleRefCount(*this);
		}
	}

	void Handle::DecrementRefCount()
	{
		if (m_pOwner != nullptr)
		{
			m_pOwner->DecrementHandleRefCount(*this);
		}
	}
}