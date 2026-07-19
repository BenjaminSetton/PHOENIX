#pragma once

#include <glslang/Public/ShaderLang.h>

#include <string>
#include <vector>

namespace PHX
{
	// Custom glslang includer that resolves #include directives.
	// Search paths are provided at construction time. Local ("") includes first search
	// relative to the including file's directory, then fall back to the search paths.
	// System (<>) includes search only the provided search paths
	class GlslangIncluder : public glslang::TShader::Includer
	{
	public:
		GlslangIncluder(const std::vector<std::string>& searchPaths);
		~GlslangIncluder() override;

		// Searches relative to the includer's directory first, then falls back to the configured search paths
		IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override;

		// Searches only the configured search paths
		IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override;

		// Releases the resources associated with an IncludeResult
		void releaseInclude(IncludeResult* result) override;

	private:
		// Attempts to open and read a file at the given full path. Returns nullptr on failure
		IncludeResult* readIncludeFile(const std::string& fullPath, const char* headerName);

		std::vector<std::string> m_searchPaths;
	};
}
