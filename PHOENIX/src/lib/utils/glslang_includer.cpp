#include "glslang_includer.h"

#include "logger.h"
#include "sanity.h"

#include <fstream>
#include <sstream>

namespace PHX
{
	// Returns the directory portion of a file path. If the path has no directory
	// component, returns an empty string.
	static std::string GetDirectory(const std::string& filePath)
	{
		size_t pos = filePath.find_last_of("/\\");
		if (pos == std::string::npos)
		{
			return "";
		}
		return filePath.substr(0, pos);
	}

	// Joins a directory and a relative path using the platform-appropriate separator
	static std::string JoinPath(const std::string& dir, const std::string& relativePath)
	{
		if (dir.empty())
		{
			return relativePath;
		}
		return dir + "/" + relativePath;
	}

	GlslangIncluder::GlslangIncluder(const std::vector<std::string>& searchPaths) : m_searchPaths(searchPaths)
	{
	}

	GlslangIncluder::~GlslangIncluder()
	{
	}

	GlslangIncluder::IncludeResult* GlslangIncluder::includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth)
	{
		UNUSED(inclusionDepth);

		// For local includes, first search relative to the includer's directory
		if (includerName != nullptr && includerName[0] != '\0')
		{
			std::string includerDir = GetDirectory(includerName);
			std::string candidatePath = JoinPath(includerDir, headerName);
			IncludeResult* result = readIncludeFile(candidatePath, headerName);
			if (result != nullptr)
			{
				return result;
			}
		}

		// Fall back to search paths
		for (const std::string& searchPath : m_searchPaths)
		{
			std::string candidatePath = JoinPath(searchPath, headerName);
			IncludeResult* result = readIncludeFile(candidatePath, headerName);
			if (result != nullptr)
			{
				return result;
			}
		}

		LogError("Failed to resolve local shader include. Could not find local include for \"%s\"", headerName);
		return nullptr;
	}

	GlslangIncluder::IncludeResult* GlslangIncluder::includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth)
	{
		UNUSED(includerName);
		UNUSED(inclusionDepth);

		// For system includes, search only the configured search paths
		for (const std::string& searchPath : m_searchPaths)
		{
			std::string candidatePath = JoinPath(searchPath, headerName);
			IncludeResult* result = readIncludeFile(candidatePath, headerName);
			if (result != nullptr)
			{
				return result;
			}
		}

		LogError("Failed to resolve system shader include. Could not find system include for \"%s\"", headerName);
		return nullptr;
	}

	void GlslangIncluder::releaseInclude(IncludeResult* result)
	{
		if (result == nullptr)
		{
			return;
		}

		// The userData pointer owns the allocated buffer (either file content or error message)
		if (result->userData != nullptr)
		{
			std::string* pBuffer = static_cast<std::string*>(result->userData);
			ASSERT_PTR(pBuffer);
			delete pBuffer;
		}

		delete result;
	}

	GlslangIncluder::IncludeResult* GlslangIncluder::readIncludeFile(const std::string& fullPath, const char* headerName)
	{
		// Unused
		(void)headerName;

		std::ifstream file(fullPath, std::ios::in | std::ios::binary);
		if (!file.is_open())
		{
			return nullptr;
		}

		std::stringstream stream;
		stream << file.rdbuf();
		std::string* pContent = new std::string(stream.str());

		return new IncludeResult(fullPath, pContent->c_str(), pContent->length(), pContent);
	}
}
