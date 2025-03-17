#pragma once

#include <glslang/Public/ShaderLang.h>

#include "PHX/types/shader_desc.h"

namespace PHX
{
	// Conversion utils from PHX types to glslang types
	namespace GLSLANG_UTILS
	{
		EShLanguage ConvertShaderStage(SHADER_STAGE kind);
		EShOptimizationLevel ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level);
		glslang::EShSource ConvertSourceLanguage(SHADER_ORIGIN origin);
	}
}