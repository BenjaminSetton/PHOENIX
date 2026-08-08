#pragma once

// Internal shader cache: manages on-disk cache files for compiled shaders.
// This class provides cache-only functionality (load from cache, write to cache).
// The orchestration logic (read file, parse includes, hash, check cache, compile
// on miss) lives in LoadOrCompileShader in phx.cpp.

#include <string>
#include <vector>

#include "BSL/integral_types.h"
#include "BSL/serialization.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	class ShaderCache
	{
	public:
		ShaderCache() = default;

		// Returns whether shader caching is enabled
		bool IsCacheEnabled() const;

		// Derives SHADER_ORIGIN from a file extension
		SHADER_ORIGIN GetOriginFromFilePath(const std::string& filePath) const;

		// Computes the cache file path from (filePath, stage)
		std::string ComputeCacheFilePath(const std::string& sourceFilePath, SHADER_STAGE stage) const;

		// Attempts to load a shader from the cache file.
		// Returns true on hit (fills outResult), false on miss or corrupt cache.
		bool TryLoadFromCache(const std::string& cacheFilePath, u32 expectedContentHash, SHADER_STAGE expectedStage, CompiledShader& outResult);

		// Writes a compiled shader to the cache file.
		STATUS_CODE WriteToCache(const std::string& sourceFilePath, SHADER_STAGE stage, SHADER_ORIGIN origin, int32_t compileTarget, u32 contentHash, const CompiledShader& compiled);

	private:
		std::string GetCacheDirectory() const;
	};
}
