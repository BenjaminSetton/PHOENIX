#pragma once

namespace PHX
{
	enum class BUFFER_USAGE
	{
		UNIFORM_BUFFER = 0,
		STORAGE_BUFFER,
		INDEX_BUFFER,	// Index buffers currently only support 32-bit index types!
		VERTEX_BUFFER,
		INDIRECT_BUFFER
	};
}