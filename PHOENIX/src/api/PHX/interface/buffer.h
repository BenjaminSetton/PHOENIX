#pragma once

#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"

#include "PHX/interface/handle.h"

namespace PHX
{
	struct BufferCreateInfo
	{
		const char* pName;
		u64 sizeBytes;
		BUFFER_USAGE bufferUsage;
	};

	struct BufferHandle : public Handle
	{
		DECLARE_PHX_HANDLE(BufferHandle);

		const char* GetName() const;
		BUFFER_USAGE GetUsage() const;
		u64 GetSize() const;
	};
}