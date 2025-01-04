#pragma once

#include <shaderc/shaderc.hpp>

#include "PHX/types/shader_desc.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		shaderc_shader_kind ConvertShaderKind(SHADER_KIND kind);
		shaderc_optimization_level ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level);
		shaderc_source_language ConvertSourceLanguage(SHADER_ORIGIN origin);
	}
}