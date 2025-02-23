#pragma once

#include "PHX/types/integral_types.h"
#include "PHX/types/texture_desc.h"

namespace PHX
{
	u32 CalculateMipLevelsFromSize(u32 width, u32 height);
	u32 GetBaseFormatSize(BASE_FORMAT format); // Returns the size in bytes
}