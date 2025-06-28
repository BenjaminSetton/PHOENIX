#pragma once

#include <memory>

#include "integral_types.h"
#include "vec_types.h"

namespace PHX
{
	enum class SHADER_STAGE : u32
	{
		VERTEX = 0,
		GEOMETRY,
		FRAGMENT,
		COMPUTE,

		MAX
	};

	enum SHADER_STAGE_FLAG : u32
	{
		SHADER_STAGE_FLAG_VERTEX   = (1 << static_cast<u32>(SHADER_STAGE::VERTEX)),
		SHADER_STAGE_FLAG_GEOMETRY = (1 << static_cast<u32>(SHADER_STAGE::GEOMETRY)),
		SHADER_STAGE_FLAG_FRAGMENT = (1 << static_cast<u32>(SHADER_STAGE::FRAGMENT)),
		SHADER_STAGE_FLAG_COMPUTE  = (1 << static_cast<u32>(SHADER_STAGE::COMPUTE)),
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

		MAX
	};

	struct ShaderUniformData
	{
		const char* name		= nullptr;
		ShaderStageFlags stages	= 0;
		u32 size				= 0;
		u32 index				= 0;
	};

	struct ShaderReflectionData
	{
		bool isValid										= false;

		std::shared_ptr<ShaderUniformData[]> pUniformData	= nullptr;
		u32 uniformCount									= 0;

		Vec3u localSize										= Vec3u(0); // Only valid for compute shaders
	};

	struct ShaderSourceData
	{
		const char* data = nullptr;
		const char* entryPoint = nullptr;
		SHADER_STAGE stage = SHADER_STAGE::MAX;
		SHADER_ORIGIN origin = SHADER_ORIGIN::MAX;
		SHADER_OPTIMIZATION_LEVEL optimizationLevel = SHADER_OPTIMIZATION_LEVEL::NONE;
		bool performReflection = true;
	};

	struct CompiledShader
	{
		std::shared_ptr<u32[]> data				= nullptr;
		u32 size								= 0;

		ShaderReflectionData reflectionData;
	};
}