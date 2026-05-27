#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/handle_types.h"

namespace PHX
{
	// Forward declarations
	class IRenderDevice;
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

		bool IsValid() const;

	private:
		void Reset();
		void PopulateHandle(IRenderDevice* pDevice, u32 index, u8 generation);
		bool IsSame(const Handle& handleA, const Handle& handleB) const;

		void IncrementRefCount();
		void DecrementRefCount();

	protected:
		IRenderDevice* m_pRenderDevice;
		u32 m_index;
		u8 m_generation;
		HANDLE_TYPE m_type;
	};

	static const Handle INVALID_HANDLE;
}