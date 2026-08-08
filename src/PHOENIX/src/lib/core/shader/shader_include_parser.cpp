#include "shader_include_parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "BSL/crc32.h"
#include "BSL/logger.h"

using namespace BSL;

namespace PHX
{
	// Get the directory portion of a path (including trailing separator)
	static std::string GetDirectory(const std::string& filePath)
	{
		size_t pos = filePath.find_last_of("/\\");
		if (pos == std::string::npos)
		{
			return ".";
		}
		return filePath.substr(0, pos);
	}

	// Read a text file into a string. Returns false on failure.
	static bool ReadFileContent(const std::string& path, std::string& outContent)
	{
		std::ifstream file(path, std::ios::in);
		if (!file.is_open())
		{
			LogError("Failed to read shader file \"%s\". Could not open file!", path.c_str());
			return false;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		outContent = buffer.str();
		return true;
	}

	bool ReadAndParseShaderFile(const std::string& filePath, const std::vector<std::string>& searchPaths, std::string& outMainContent, std::vector<ResolvedInclude>& outIncludes)
	{
		if (!ReadFileContent(filePath, outMainContent))
		{
			return false;
		}

		std::string baseDir = GetDirectory(filePath);
		std::unordered_set<std::string> visited;

		// Recursive lambda for parsing includes
		std::function<void(const std::string&, const std::string&)> parseRecursive =
			[&](const std::string& src, const std::string& currDir)
		{
			std::istringstream stream(src);
			std::string line;
			while (std::getline(stream, line))
			{
				// Find #include "..." pattern
				size_t includePos = line.find("#include");
				if (includePos == std::string::npos)
					continue;

				size_t quoteStart = line.find('"', includePos);
				if (quoteStart == std::string::npos)
					continue;

				size_t quoteEnd = line.find('"', quoteStart + 1);
				if (quoteEnd == std::string::npos)
					continue;

				std::string includeName = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

				// Try to resolve the include path
				std::string resolvedPath;
				bool found = false;

				// First, search relative to the current directory
				{
					std::string candidate = currDir + "/" + includeName;
					std::ifstream testFile(candidate);
					if (testFile.is_open())
					{
						resolvedPath = candidate;
						found = true;
					}
				}

				// Then, search in the provided search paths
				if (!found)
				{
					for (const std::string& searchPath : searchPaths)
					{
						std::string candidate = searchPath + "/" + includeName;
						std::ifstream testFile(candidate);
						if (testFile.is_open())
						{
							resolvedPath = candidate;
							found = true;
							break;
						}
					}
				}

				if (!found)
					continue;

				if (visited.count(resolvedPath) > 0)
					continue;

				visited.insert(resolvedPath);

				// Read the include file content and add to results
				ResolvedInclude inc;
				inc.path = resolvedPath;
				if (!ReadFileContent(resolvedPath, inc.content))
					continue;

				outIncludes.push_back(inc);

				// Recursively parse the included file
				std::string includeDir = GetDirectory(resolvedPath);
				parseRecursive(inc.content, includeDir);
			}
		};

		parseRecursive(outMainContent, baseDir);
		return true;
	}

	u32 ComputeShaderContentHash(const std::string& mainFilePath, const std::string& mainContent, const std::vector<ResolvedInclude>& includes, int32_t compileTarget)
	{
		// Build a hash buffer: main file path + content, then sorted includes (path + content), then compile target
		std::string buffer;
		buffer.reserve(mainFilePath.size() + mainContent.size() + 256);

		buffer += mainFilePath;
		buffer += '\n';
		buffer += mainContent;
		buffer += '\n';

		// Sort include paths lexicographically for deterministic order
		std::vector<const ResolvedInclude*> sortedIncludes;
		sortedIncludes.reserve(includes.size());
		for (const ResolvedInclude& inc : includes)
		{
			sortedIncludes.push_back(&inc);
		}
		std::sort(sortedIncludes.begin(), sortedIncludes.end(),
			[](const ResolvedInclude* a, const ResolvedInclude* b) { return a->path < b->path; });

		for (const ResolvedInclude* inc : sortedIncludes)
		{
			buffer += inc->path;
			buffer += '\n';
			buffer += inc->content;
			buffer += '\n';
		}

		// Append compile target as raw bytes
		buffer.append(reinterpret_cast<const char*>(&compileTarget), sizeof(int32_t));

		return BSL::HashCRC32(buffer.c_str());
	}
}
