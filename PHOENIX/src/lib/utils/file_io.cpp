
#include "utils/file_io.h"

namespace PHX
{
	FileIO::FileIO(const char* filePath)
	{
		m_stream.open(filePath, std::ios::out);
	}

	FileIO::~FileIO()
	{
		Close();
	}

	bool FileIO::IsOpen() const
	{
		return m_stream.is_open();
	}

	bool FileIO::Write(const char* data, u32 length)
	{
		m_stream.write(data, static_cast<std::streamsize>(length));
		return m_stream.good();
	}

	u32 FileIO::Read(char* data, u32 length)
	{
		m_stream.read(data, static_cast<std::streamsize>(length));
		return static_cast<u32>(m_stream.gcount());
	}

	u32 FileIO::GetSize() const
	{
		std::streampos currentPos = m_stream.tellg();
		m_stream.seekg(0, std::ios::end);
		std::streampos size = m_stream.tellg();
		m_stream.seekg(currentPos);
		return static_cast<u32>(size);
	}

	bool FileIO::Seek(u32 position)
	{
		std::streamoff offset = static_cast<std::streamoff>(position);
		m_stream.seekg(offset, std::ios::beg);
		m_stream.seekp(offset, std::ios::beg);
		return m_stream.good();
	}

	void FileIO::Close()
	{
		if (m_stream.is_open())
		{
			m_stream.close();
		}
	}
}