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

		u32 GetIndex() const;
		HANDLE_TYPE GetType() const;

	private:
		void Reset();
		void PopulateHandle(HandleOwner* pOwner, u32 index);
		bool IsSame(const Handle& handleA, const Handle& handleB) const;

		void IncrementRefCount();
		void DecrementRefCount();

	protected:
		HandleOwner* m_pOwner;
		u32 m_index;
		HANDLE_TYPE m_type;
	};

	static const Handle INVALID_HANDLE;

#define DECLARE_PHX_HANDLE(HandleType)				\
	HandleType();									\
	HandleType(const Handle& other);				\
	~HandleType();									\
	HandleType(const HandleType& other);			\
	HandleType& operator=(const HandleType& other);	\
	HandleType(HandleType&& other) noexcept;

#define DEFINE_PHX_HANDLE(HandleType, HandleEnum)										\
	HandleType::HandleType() : Handle(HandleEnum) { }									\
	HandleType::HandleType(const Handle& other) : Handle(other) { }						\
	HandleType::~HandleType() { }														\
	HandleType::HandleType(const HandleType& other) : Handle(other) { }					\
	HandleType& HandleType::operator=(const HandleType& other) {						\
		if (this == &other) return *this;												\
		Handle::operator=(other);														\
		return *this;																	\
	}																					\
	HandleType::HandleType(HandleType&& other) noexcept : Handle(std::move(other)) { }
}