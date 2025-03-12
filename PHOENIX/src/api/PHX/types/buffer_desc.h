#pragma once

namespace PHX
{
	enum class BUFFER_USAGE
	{
		TRANSFER_SRC = 0,
		TRANSFER_DST,
		UNIFORM_BUFFER,
		STORAGE_BUFFER,
		INDEX_BUFFER,	// Index buffers currently only support 32-bit index types!
		VERTEX_BUFFER,
		INDIRECT_BUFFER
	};
}