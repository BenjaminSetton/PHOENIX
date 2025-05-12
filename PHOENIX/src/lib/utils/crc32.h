#pragma once

#include "PHX/types/integral_types.h"

namespace PHX
{
	typedef u32 CRC32;

	void InitCRC32();
	CRC32 HashCRC32(const char* str);
}