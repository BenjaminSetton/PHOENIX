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

	struct GeometryData
	{
		GEOMETRY_TYPE type = GEOMETRY_TYPE::TRIANGLES;

		// Triangle geometry
		BufferHandle vertexBuffer = INVALID_HANDLE;
		BufferHandle indexBuffer  = INVALID_HANDLE;

		u32 vertexCount = 0;
		u32 indexCount  = 0;
		u32 vertexStride = 0; // Stride in bytes between vertices
		INDEX_TYPE indexType = INDEX_TYPE::U32;

		// AABB geometry
		BufferHandle aabbBuffer = INVALID_HANDLE;
		u32 aabbCount = 0;
	};
}
