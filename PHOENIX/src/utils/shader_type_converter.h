#pragma once

#include <glslang/Public/ShaderLang.h>

#include "PHX/types/shader_desc.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		EShLanguage ConvertShaderKind(SHADER_KIND kind);
		EShOptimizationLevel ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level);
		glslang::EShSource ConvertSourceLanguage(SHADER_ORIGIN origin);
	}
}