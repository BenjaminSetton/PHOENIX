#pragma once

#include <memory>
#include <string>
#include <vector>

#include "BSL/integral_types.h"
#include "BSL/sanity.h"
#include "BSL/vec_types.h"
#include "texture_desc.h"

namespace PHX
{
	enum class SHADER_STAGE : u32
	{
		VERTEX = 0,
		GEOMETRY,
		FRAGMENT,
		COMPUTE,
		RAYGEN,
		INTERSECTION,
		ANY_HIT,
		CLOSEST_HIT,
		MISS,
		CALLABLE,
		TESSELLATION_CONTROL,
		TESSELLATION_EVALUATION,

		MAX
	};

	enum SHADER_STAGE_FLAG : u32
	{
		SHADER_STAGE_FLAG_VERTEX                  = (1 << static_cast<u32>(SHADER_STAGE::VERTEX)),
		SHADER_STAGE_FLAG_GEOMETRY                = (1 << static_cast<u32>(SHADER_STAGE::GEOMETRY)),
		SHADER_STAGE_FLAG_FRAGMENT                = (1 << static_cast<u32>(SHADER_STAGE::FRAGMENT)),
		SHADER_STAGE_FLAG_COMPUTE                 = (1 << static_cast<u32>(SHADER_STAGE::COMPUTE)),
		SHADER_STAGE_FLAG_RAYGEN                  = (1 << static_cast<u32>(SHADER_STAGE::RAYGEN)),
		SHADER_STAGE_FLAG_INTERSECTION            = (1 << static_cast<u32>(SHADER_STAGE::INTERSECTION)),
		SHADER_STAGE_FLAG_ANY_HIT                 = (1 << static_cast<u32>(SHADER_STAGE::ANY_HIT)),
		SHADER_STAGE_FLAG_CLOSEST_HIT             = (1 << static_cast<u32>(SHADER_STAGE::CLOSEST_HIT)),
		SHADER_STAGE_FLAG_MISS                    = (1 << static_cast<u32>(SHADER_STAGE::MISS)),
		SHADER_STAGE_FLAG_CALLABLE                = (1 << static_cast<u32>(SHADER_STAGE::CALLABLE)),
		SHADER_STAGE_FLAG_TESSELLATION_CONTROL    = (1 << static_cast<u32>(SHADER_STAGE::TESSELLATION_CONTROL)),
		SHADER_STAGE_FLAG_TESSELLATION_EVALUATION = (1 << static_cast<u32>(SHADER_STAGE::TESSELLATION_EVALUATION)),
	};
	using ShaderStageFlags = u32;

	enum class SHADER_OPTIMIZATION_LEVEL
	{
		NONE = 0,              // No optimization
		PERFORMANCE_FAST,      // Perform _some_ optimization for performance. Faster compile time, but less optimized binary
		PERFORMANCE_FULL,      // Perform as much optimization for performance as possible. Slowest compile time, but most optimized binary
		SIZE,                  // Optimize for smallest binary size

		MAX
	};

	enum class SHADER_ORIGIN
	{
		HLSL = 0,
		GLSL,
		SLANG,

		MAX
	};

	struct ShaderUniformData
	{
		const char* name        = nullptr;
		ShaderStageFlags stages = 0;
		u32 size                = 0;
		u32 binding             = 0;
		u32 offset              = 0;
	};

	struct ShaderIOData
	{
		const char* name   = nullptr;
		BASE_FORMAT format = BASE_FORMAT::INVALID;
		u32 location       = 0;
		u32 binding        = 0;
	};

	struct ShaderReflectionData
	{
		bool isValid                                        = false;

		std::shared_ptr<ShaderUniformData[]> uniforms       = nullptr;
		u32 uniformCount                                    = 0;

		std::shared_ptr<ShaderIOData[]> inputs              = nullptr;
		u32 inputCount                                      = 0;

		std::shared_ptr<ShaderIOData[]> outputs             = nullptr;
		u32 outputCount                                     = 0;

		BSL::Vec3u localSize                                = BSL::Vec3u(0); // Only valid for compute shaders

		// Owned string storage for name pointers when loaded from cache.
		// When compiled via Slang, names point into Slang's internal string pool and this is null.
		// When deserialized from cache, names point into this arena.
		TECHDEBT("Remove this shit")
		std::shared_ptr<std::vector<char>> stringArena      = nullptr;
	};

	struct ShaderSourceData
	{
		const char* data							= nullptr;
		const char* entryPoint						= nullptr;
		SHADER_STAGE stage							= SHADER_STAGE::MAX;
		SHADER_ORIGIN origin						= SHADER_ORIGIN::MAX;
		SHADER_OPTIMIZATION_LEVEL optimizationLevel	= SHADER_OPTIMIZATION_LEVEL::NONE;
		bool performReflection						= true;
		const char** includePaths					= nullptr;	// Array of include search directory paths
		u32 includePathCount						= 0;		// Number of entries in includePaths

		// Optional cache metadata. When sourceFilePath is non-null, CompileShader
		// writes the result to the shader cache after successful compilation.
		const char* sourceFilePath					= nullptr;	// Original file path (for cache key). Null = no cache write
		u32 contentHash								= 0;		// CRC32 of source + includes + compile target
	};

	struct ShaderFileSourceData
	{
		const char* filePath = nullptr;
		const char* entryPoint = "main";
		SHADER_STAGE stage = SHADER_STAGE::MAX;
		SHADER_OPTIMIZATION_LEVEL optimizationLevel = SHADER_OPTIMIZATION_LEVEL::NONE;
		bool performReflection = true;
		const char** includePaths = nullptr; // Additional include search directories
		u32 includePathCount = 0;
	};

	struct ResolvedShaderIncludes
	{
		std::string mainFilePath;
		std::vector<std::string> includeFilePaths; // All transitive includes, full paths
	};

	struct CompiledShader
	{
		std::shared_ptr<u32[]> data				= nullptr;
		u32 size								= 0;

		ShaderReflectionData reflectionData;
	};
}