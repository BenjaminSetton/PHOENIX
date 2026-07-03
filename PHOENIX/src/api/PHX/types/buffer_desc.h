#pragma once

namespace PHX
{
	enum class BUFFER_USAGE
	{
		UNIFORM_BUFFER = 0,
		STORAGE_BUFFER,
		INDEX_BUFFER,
		VERTEX_BUFFER,
		INDIRECT_BUFFER
	};

	enum class INDEX_TYPE
	{
		U16 = 0,
		U32,

		MAX
	};
}