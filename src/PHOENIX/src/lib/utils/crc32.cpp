
#include <cstring>

#include "utils/crc32.h"
#include "utils/logger.h"

// Implementations taken from: https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635

namespace PHX
{
	static constexpr u32 CRC_TABLE_SIZE = 256;
	static CRC32 s_table[CRC_TABLE_SIZE];

	void InitCRC32()
	{
		u32 polynomial = 0xEDB88320;
		for (u32 i = 0; i < CRC_TABLE_SIZE; i++)
		{
			u32 c = i;
			for (size_t j = 0; j < 8; j++)
			{
				if (c & 1) {
					c = polynomial ^ (c >> 1);
				}
				else {
					c >>= 1;
				}
			}
			s_table[i] = c;
		}
	}

	CRC32 HashCRC32(const char* str)
	{
		u32 c = 0xFFFFFFFF;
		const u32 strLen = static_cast<u32>(strlen(str));
		for (size_t i = 0; i < strLen; i++)
		{
			c = s_table[(c ^ str[i]) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFF;
	}
}