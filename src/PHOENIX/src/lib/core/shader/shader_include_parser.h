#pragma once

#include <string>
#include <vector>

#include "BSL/integral_types.h"
#include "BSL/sanity.h"

// Forward declarations
namespace Slang
{
	typedef int32_t SlangCompileTarget;
}

TECHDEBT("Investigate if Slang reflection can return resolved include paths directly, replacing this string-matching parser")

namespace PHX
{
	struct ResolvedInclude
	{
		std::string path;     // Full resolved path
		std::string content;  // File content (cached for hashing)
	};

	// Returns all the files involved in the include chain recursively, including the specified shader
	// Returns true on success, false otherwise
	bool ReadAndParseShaderFile(const std::string& filePath, const std::vector<std::string>& searchPaths, std::string& outMainContent, std::vector<ResolvedInclude>& outIncludes);

	// Computes the CRC32 content hash of the entire shader (plus included files)
	u32 ComputeShaderContentHash(const std::string& mainFilePath, const std::string& mainContent, const std::vector<ResolvedInclude>& includes, int32_t compileTarget);
}
