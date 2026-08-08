#include "shader_cache.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "BSL/crc32.h"
#include "BSL/file_io.h"
#include "BSL/logger.h"
#include "BSL/sanity.h"
#include "PHX/phx.h"
#include "core/global_settings.h"
#include "shader_serialization.h"

TECHDEBT("Make all serialization paths atomic (write .tmp + rename) — applies to shader cache, asset cache, and any future serialization")

using namespace BSL;

namespace PHX
{
	static constexpr u32 PHXC_MAGIC = BSL::MakeMagicNumber("PHXC");
	static constexpr u32 PHXC_VERSION = 1;
	static constexpr const char* PHXC_EXT = ".phxc";

	struct CacheFileHeader
	{
		u32 magic;             // PHXC_MAGIC
		u32 version;           // PHXC_VERSION
		u32 phxLibraryVersion; // GetFullVersion() at write time
		u32 stage;             // SHADER_STAGE
		u32 backendAPI;        // GRAPHICS_API
		u32 backendMajor;
		u32 backendMinor;
		u32 origin;            // SHADER_ORIGIN
		u32 compileTarget;     // SlangCompileTarget
		u32 contentHash;       // CRC32 of source + includes + compileTarget
		u32 sourcePathLen;     // Length of the source path string
	};

	static const char* GetStageName(SHADER_STAGE stage)
	{
		switch (stage)
		{
		case SHADER_STAGE::VERTEX:                  return "vert";
		case SHADER_STAGE::GEOMETRY:                return "geom";
		case SHADER_STAGE::FRAGMENT:                return "frag";
		case SHADER_STAGE::COMPUTE:                 return "comp";
		case SHADER_STAGE::RAYGEN:                  return "rgen";
		case SHADER_STAGE::INTERSECTION:            return "rint";
		case SHADER_STAGE::ANY_HIT:                 return "rhit";
		case SHADER_STAGE::CLOSEST_HIT:             return "rchit";
		case SHADER_STAGE::MISS:                    return "rmiss";
		case SHADER_STAGE::CALLABLE:                return "rcall";
		case SHADER_STAGE::TESSELLATION_CONTROL:    return "tesc";
		case SHADER_STAGE::TESSELLATION_EVALUATION: return "tese";
		default:                                    return "unknown";
		}
	}

	bool ShaderCache::IsCacheEnabled() const
	{
		const Settings& settings = GlobalSettings::Get().GetSettings();
		return settings.enableShaderCache && settings.cacheDirectory != nullptr;
	}

	std::string ShaderCache::GetCacheDirectory() const
	{
		const Settings& settings = GlobalSettings::Get().GetSettings();
		if (settings.cacheDirectory == nullptr)
		{
			return "";
		}
		return std::string(settings.cacheDirectory) + "/shaders";
	}

	SHADER_ORIGIN ShaderCache::GetOriginFromFilePath(const std::string& filePath) const
	{
		size_t dotPos = filePath.find_last_of('.');
		if (dotPos == std::string::npos)
		{
			return SHADER_ORIGIN::SLANG;
		}

		std::string ext = filePath.substr(dotPos + 1);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == "slang")
		{
			return SHADER_ORIGIN::SLANG;
		}
		if (ext == "hlsl")
		{
			return SHADER_ORIGIN::HLSL;
		}

		// Default to GLSL
		return SHADER_ORIGIN::GLSL;
	}

	std::string ShaderCache::ComputeCacheFilePath(const std::string& sourceFilePath, SHADER_STAGE stage) const
	{
		std::string cacheDir = GetCacheDirectory();
		std::string stageName = GetStageName(stage);

		// Strip "." and ".." segments, join remaining parts with underscores
		std::string clean;
		for (const std::filesystem::path& part : std::filesystem::path(sourceFilePath))
		{
			if (part == "." || part == "..") continue;
			if (!clean.empty()) clean += "_";
			clean += part.string();
		}

		return cacheDir + "/" + clean + "_" + stageName + PHXC_EXT;
	}

	bool ShaderCache::TryLoadFromCache(const std::string& cacheFilePath, u32 expectedContentHash, SHADER_STAGE expectedStage, CompiledShader& outResult)
	{
		std::ifstream file(cacheFilePath, std::ios::binary);
		if (!file.is_open())
		{
			return false;
		}

		CacheFileHeader header{};
		file.read(reinterpret_cast<char*>(&header), sizeof(CacheFileHeader));
		if (!file.good())
		{
			LogError("Failed to load shader from cache. Could not open cache file: \"%s\"", cacheFilePath.c_str(), cacheFilePath.c_str());
			return false;
		}

		if (header.magic != PHXC_MAGIC)
		{
			LogError("Failed to load shader from cache. Expected magic number %u, got %u: %s", PHXC_MAGIC, header.magic, cacheFilePath.c_str());
			return false;
		}

		if (header.version != PHXC_VERSION)
		{
			LogError("Failed to load shader from cache. Expected header version %u, got %u: \"%s\"", PHXC_VERSION, header.version, cacheFilePath.c_str());
			return false;
		}

		if (header.contentHash != expectedContentHash)
		{
			LogError("Failed to load shader from cache. Content hash mismatch: \"%s\"", cacheFilePath.c_str());
			return false;
		}

		if (static_cast<SHADER_STAGE>(header.stage) != expectedStage)
		{
			LogError("Failed to load shader from cache. Cache stage mismatch: \"%s\"", cacheFilePath.c_str());
			return false;
		}

		// Skip the source path string
		file.ignore(header.sourcePathLen);
		if (!file.good())
		{
			LogError("Failed to load shader from cache. Cache file corrupt (truncated source path): \"%s\"", cacheFilePath.c_str());
			return false;
		}

		// Read the PHXS payload
		std::streampos payloadStart = file.tellg();
		file.seekg(0, std::ios::end);
		std::streampos fileEnd = file.tellg();
		u64 payloadSize = static_cast<u64>(fileEnd - payloadStart);
		file.seekg(payloadStart);

		std::vector<u8> phxsBlob(payloadSize);
		file.read(reinterpret_cast<char*>(phxsBlob.data()), static_cast<std::streamsize>(payloadSize));
		if (!file.good())
		{
			LogError("Failed to load shader from cache. Cache file corrupt (truncated payload): \"%s\"", cacheFilePath.c_str());
			return false;
		}

		SHADER_STAGE deserializedStage;
		STATUS_CODE res = DeserializeCompiledShader(phxsBlob.data(), phxsBlob.size(), outResult, deserializedStage);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to load shader from cache. Failed to deserialize shader cache payload: \"%s\"", cacheFilePath.c_str());
			return false;
		}

		return true;
	}

	STATUS_CODE ShaderCache::WriteToCache(const std::string& sourceFilePath, SHADER_STAGE stage, SHADER_ORIGIN origin, int32_t compileTarget, u32 contentHash, const CompiledShader& compiled)
	{
		if (!IsCacheEnabled())
		{
			// No work to do
			return STATUS_CODE::SUCCESS;
		}

		std::string cacheFilePath = ComputeCacheFilePath(sourceFilePath, stage);

		// Serialize the compiled shader into a PHXS blob
		std::vector<u8> phxsBlob;
		STATUS_CODE res = SerializeCompiledShader(compiled, stage, phxsBlob);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to write to shader cache. Could not serialize shader for cache write: \"%s\"", cacheFilePath.c_str());
			return res;
		}

		// Build the cache file header
		const Settings& settings = GlobalSettings::Get().GetSettings();
		CacheFileHeader header{};
		header.magic             = PHXC_MAGIC;
		header.version           = PHXC_VERSION;
		header.phxLibraryVersion = GetFullVersion();
		header.stage             = static_cast<u32>(stage);
		header.backendAPI        = static_cast<u32>(settings.backendAPI);
		header.backendMajor      = static_cast<u32>(settings.backendAPIMajorVersion);
		header.backendMinor      = static_cast<u32>(settings.backendAPIMinorVersion);
		header.origin            = static_cast<u32>(origin);
		header.compileTarget     = static_cast<u32>(compileTarget);
		header.contentHash       = contentHash;
		header.sourcePathLen     = static_cast<u32>(sourceFilePath.size());

		// Write the cache file
		FileIO io(cacheFilePath.c_str(), true);
		if (!io.IsOpen())
		{
			LogError("Failed to write to shader cache. Could not open shader cache file for writing: \"%s\"", cacheFilePath.c_str());
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (!io.Write(reinterpret_cast<const char*>(&header), sizeof(CacheFileHeader)) ||
			!io.Write(sourceFilePath.data(), static_cast<u32>(sourceFilePath.size())) ||
			!io.Write(reinterpret_cast<const char*>(phxsBlob.data()), static_cast<u32>(phxsBlob.size())))
		{
			LogError("Failed to write shader cache file: \"%s\"", cacheFilePath.c_str());
			return STATUS_CODE::ERR_INTERNAL;
		}

		LogInfo("Cached shader: \"%s\"", cacheFilePath.c_str());
		return STATUS_CODE::SUCCESS;
	}
}
