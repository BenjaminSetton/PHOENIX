#pragma once

#include <atomic>

#include "PHX/types/integral_types.h"

namespace PHX
{
	// TODO - Move to lib along with handles
	class RefCounted
	{
	public:

		RefCounted();
		~RefCounted();

		i32 GetRefCount() const;
		void IncrementRefCount();
		void DecrementRefCount();

	private:
		std::atomic<i32> m_refCount;
	};
}