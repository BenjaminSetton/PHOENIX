#pragma once

#include <glslang/Public/ShaderLang.h>

#include "PHX/types/shader_desc.h"

namespace PHX
{
	// Conversion utils between PHX types and glslang types
	namespace GLSLANG_UTILS
	{
		EShLanguage ConvertShaderStage(SHADER_STAGE kind);
		EShOptimizationLevel ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level);
		glslang::EShSource ConvertSourceLanguage(SHADER_ORIGIN origin);

		ShaderStageFlags ConvertShaderStageFlags(EShLanguageMask stageMask);
	}
}