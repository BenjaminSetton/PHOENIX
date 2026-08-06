#pragma once

#include <vector>

#include "BSL/integral_types.h"
#include "BSL/sanity.h"

namespace PHX
{
	// Stores interface types in a slot-based list, where indices internally get reused so existing
	// handles are not invalidated when other handles are deleted.
	// ObjectT must derive from RefCounted
	template<typename InterfaceT>
	class HandleList
	{
	public:

		HandleList() = default;
		~HandleList() = default;

		// Stores pObj in a free slot (reused if available, otherwise appended) and
		// returns the stable index of that slot
		u32 Allocate(InterfaceT* pObj)
		{
			u32 index;
			if (!m_freeList.empty())
			{
				index = m_freeList.back();
				m_freeList.pop_back();
				m_slots[index] = pObj;
			}
			else
			{
				index = static_cast<u32>(m_slots.size());
				m_slots.push_back(pObj);
			}
			return index;
		}

		// Returns the object at the given index or nullptr if the index is out of
		// range or the slot has been freed
		InterfaceT* Resolve(u32 index) const
		{
			if (index >= static_cast<u32>(m_slots.size()))
			{
				LogError("Failed to resolve handle. Index %u is out of range!", index);
				return nullptr;
			}

			InterfaceT* pObj = m_slots[index];
			if (pObj == nullptr)
			{
				LogError("Failed to resolve handle. Slot at index %u has been freed!", index);
			}
			return pObj;
		}

		void IncrementRefCount(u32 index)
		{
			InterfaceT* pObj = Get(index);
			if (pObj != nullptr)
			{
				pObj->IncrementRefCount();
			}
		}

		// Decrements the object's ref count and deletes it once the
		// count reaches zero. Slots from deleted objects are reused
		void DecrementRefCount(u32 index)
		{
			InterfaceT* pObj = Get(index);
			if (pObj != nullptr)
			{
				pObj->DecrementRefCount();
				if (pObj->GetRefCount() <= 0)
				{
					Free(index);
				}
			}
		}

		// Decrements the object's ref count without ever deleting it. Used for objects
		// whose lifetime is managed elsewhere (e.g. render passes owned by the render graph)
		void DecrementRefCountNoDelete(u32 index)
		{
			InterfaceT* pObj = Get(index);
			if (pObj != nullptr)
			{
				pObj->DecrementRefCount();
			}
		}

		// Replaces the object at the given index with pObj. The old object is deleted
		// and the new object inherits the old object's ref count, so existing handles
		// remain valid and resolve to the new object. Does NOT touch the free list.
		// Used for things like hot-reloading shaders
		void Replace(u32 index, InterfaceT* pObj)
		{
			if (index >= static_cast<u32>(m_slots.size()))
			{
				LogError("Failed to replace handle. Index %u is out of range!", index);
				return;
			}

			InterfaceT* pOldObj = m_slots[index];
			i32 oldRefCount = 0;
			if (pOldObj != nullptr)
			{
				oldRefCount = pOldObj->GetRefCount();
			}

			SAFE_DEL(pOldObj);
			m_slots[index] = pObj;
			pObj->SetRefCount(oldRefCount);
		}

		void DeleteAll()
		{
			for (InterfaceT*& pObj : m_slots)
			{
				SAFE_DEL(pObj);
			}
			m_slots.clear();
			m_freeList.clear();
		}

		// Returns number of slots, including free ones
		u32 Size() const
		{
			return static_cast<u32>(m_slots.size());
		}

		// Returns number of active (non-freed) slots
		u32 GetActiveCount() const
		{
			return static_cast<u32>(m_slots.size() - m_freeList.size());
		}

		// Returns when no slots are allocated
		bool Empty() const
		{
			return Size() == 0;
		}

		// Returns the object at the given index
		InterfaceT* Get(u32 index) const
		{
			if (index < static_cast<u32>(m_slots.size()))
			{
				return m_slots[index];
			}
			return nullptr;
		}

	private:

		void Free(u32 index)
		{
			BSL::LogDebug("Freeing object at 0x%p", m_slots[index]);
			SAFE_DEL(m_slots[index]);
			m_freeList.push_back(index);
		}

		std::vector<InterfaceT*> m_slots;
		std::vector<u32> m_freeList;
	};
}
