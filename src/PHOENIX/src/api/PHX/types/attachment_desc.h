#pragma once

namespace PHX
{
	enum class ATTACHMENT_TYPE
	{
		INVALID = 0,

		COLOR,
		DEPTH,
		STENCIL,
		DEPTH_STENCIL,
		RESOLVE
	};

	enum class ATTACHMENT_STORE_OP
	{
		INVALID = 0,

		STORE,
		IGNORE
	};

	enum class ATTACHMENT_LOAD_OP
	{
		INVALID = 0,

		LOAD,
		CLEAR,
		IGNORE
	};
}