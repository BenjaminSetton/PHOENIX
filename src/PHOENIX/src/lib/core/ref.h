#pragma once

#include <atomic>

#include "BSL/integral_types.h"

namespace PHX
{
	class RefCounted
	{
	public:

		RefCounted();
		~RefCounted();
		RefCounted(const RefCounted& other);
		RefCounted& operator=(const RefCounted& other);
		RefCounted(RefCounted&& other) noexcept;

		inline i32 GetRefCount() const  { return m_refCount.load(std::memory_order_acquire); }
		inline void IncrementRefCount() { m_refCount.fetch_add(1, std::memory_order_relaxed); }
		inline void DecrementRefCount() { m_refCount.fetch_sub(1, std::memory_order_acq_rel); }
		inline void SetRefCount(i32 count) { m_refCount.store(count, std::memory_order_release); }

	private:
		std::atomic<i32> m_refCount;
	};
}