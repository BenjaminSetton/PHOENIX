#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/types/buffer_desc.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/vec_types.h"

namespace PHX
{
	enum class GEOMETRY_TYPE
	{
		TRIANGLES = 0,
		AABBS,
		
		MAX
	};

	enum GEOMETRY_FLAG : u8
	{
		GEOMETRY_FLAG_NONE                             = 0,
		GEOMETRY_FLAG_OPAQUE                           = (1 << 0),
		GEOMETRY_FLAG_NO_DUPLICATE_ANY_HIT_INVOCATION  = (1 << 1),
	};
	using GeometryFlags = u8;

	struct GeometryData
	{
		GEOMETRY_TYPE type  = GEOMETRY_TYPE::TRIANGLES;
		GeometryFlags flags = GEOMETRY_FLAG_OPAQUE;

		// Triangle geometry
		BufferHandle vertexBuffer = INVALID_HANDLE;
		BufferHandle indexBuffer  = INVALID_HANDLE;

		u32 vertexCount = 0;
		u32 indexCount  = 0;
		u32 vertexStride = 0; // Stride in bytes between vertices
		u32 firstVertex = 0;  // Offset (in vertices) into the shared vertex buffer
		u64 indexByteOffset = 0; // Offset (in bytes) into the shared index buffer
		INDEX_TYPE indexType = INDEX_TYPE::U32;

		// AABB geometry
		BufferHandle aabbBuffer = INVALID_HANDLE;
		u32 aabbCount = 0;
	};
}
