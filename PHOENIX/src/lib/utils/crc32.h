#pragma once

#include <string_view>

#include "PHX/types/integral_types.h"

namespace PHX
{
	typedef u32 CRC32;

	void InitCRC32();
	CRC32 HashCRC32(std::string_view str);
}