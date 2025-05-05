
#include "utils/crc32.h"

// Implementations taken from: https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635

namespace PHX
{
	static CRC32 s_table[256];

	void InitCRC32()
	{
		u32 polynomial = 0xEDB88320;
		for (u32 i = 0; i < 256; i++)
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

	CRC32 HashCRC32(std::string_view str)
	{
		u32 c = 0xFFFFFFFF;
		const u32 strLen = str.length();
		for (size_t i = 0; i < strLen; i++)
		{
			c = s_table[(c ^ str.at(i)) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFF;
	}
}