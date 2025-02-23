#pragma once

#include <memory>

#include "integral_types.h"

namespace PHX
{
	enum class SHADER_STAGE
	{
		VERTEX = 0,
		GEOMETRY,
		FRAGMENT,
		COMPUTE,

		MAX
	};

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

	struct ShaderSourceData
	{
		const char* data;

		SHADER_STAGE stage;
		SHADER_OPTIMIZATION_LEVEL optimizationLevel;
		SHADER_ORIGIN origin;
	};

	struct CompiledShader
	{
		std::shared_ptr<u32[]> data;
		u32 size;
	};
}