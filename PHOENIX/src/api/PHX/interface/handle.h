#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/handle_types.h"

namespace PHX
{
	// Forward declarations
	class HandleOwner;
	struct HandleAccessor;

	// DO NOT CONSTRUCT HANDLE OBJECTS DIRECTLY. INSTEAD MAKE AN INSTANCE OF DERIVED HANDLES (e.g. TextureHandle, BufferHandle, etc)
	class Handle
	{
	public:

		friend struct HandleAccessor;

		Handle();
		Handle(HANDLE_TYPE type);
		~Handle();
		Handle(const Handle& other);
		Handle& operator=(const Handle& other);
		Handle(Handle&& other) noexcept;

		bool operator==(const Handle& other) const;
		bool operator!=(const Handle& other) const;

		bool IsValid() const;

	private:
		void Reset();
		void PopulateHandle(HandleOwner* pOwner, u32 index, u8 generation);
		bool IsSame(const Handle& handleA, const Handle& handleB) const;

		void IncrementRefCount();
		void DecrementRefCount();

	protected:
		HandleOwner* m_pOwner;
		u32 m_index;
		u8 m_generation;
		HANDLE_TYPE m_type;
	};

#define DECLARE_HANDLE(HandleType)					\
	HandleType();									\
	HandleType(const Handle& other);				\
	~HandleType();									\
	HandleType(const HandleType& other);			\
	HandleType& operator=(const HandleType& other);	\
	HandleType(HandleType&& other) noexcept;

	// Move to cpp? Prefer functions (e.g. IsValid()) instead of direct reference
	static const Handle INVALID_HANDLE;
}