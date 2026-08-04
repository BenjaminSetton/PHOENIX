#pragma once

#include <fstream>

#include "PHX/types/integral_types.h"

namespace PHX
{
	// Synchronous file utilities
	// TODO - Clean up, simple wrapper for now
	class FileIO
	{
	public:
		explicit FileIO(const char* filePath);
		// TODO - Properly implement missing big 5 members
		~FileIO();

		bool IsOpen() const;

		bool Write(const char* data, u32 length);
		u32 Read(char* data, u32 length);

		u32 GetSize() const;
		bool Seek(u32 position);

		void Close();

	private:
		mutable std::fstream m_stream;
	};
}
