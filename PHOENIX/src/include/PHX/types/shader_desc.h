#pragma once

#include <memory>

#include "integral_types.h"

namespace PHX
{
	enum class SHADER_KIND
	{
		VERTEX = 0,
		GEOMETRY,
		FRAGMENT,
		COMPUTE,

		MAX
	};

	enum class SHADER_OPTIMIZATION_LEVEL
	{
		NONE = 0,   // No optimization
		O1,         // Faster optimization. Yields less optimized shader bytecode
		O2,         // Slower optimization. Yields more optimized shader bytecode

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
		const char* name;

		SHADER_KIND kind;
		SHADER_OPTIMIZATION_LEVEL optimizationLevel;
		SHADER_ORIGIN origin;
	};

	struct CompiledShader
	{
		std::shared_ptr<u32[]> data;
		u32 size;
	};
}