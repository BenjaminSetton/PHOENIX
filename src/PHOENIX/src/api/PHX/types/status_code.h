#pragma once

#include "integral_types.h"

namespace PHX
{
	// TODO - Add more detailed error codes
	enum class STATUS_CODE : u8
	{
		SUCCESS = 0,
		ERR_API,		// PHX API error. This error is caused when the API is used in an unintended way, which _directly_ causes an error
		ERR_INTERNAL	// Internal PHX error. Something went wrong in the library's logic
	};
}