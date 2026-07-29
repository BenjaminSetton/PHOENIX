#pragma once

#include <slang.h>

#include "PHX/types/shader_desc.h"
#include "PHX/types/texture_desc.h"
#include "PHX/types/settings.h"

namespace PHX
{
	namespace SLANG_UTILS
	{
		SlangStage ConvertShaderStage(SHADER_STAGE kind);
		SlangSourceLanguage ConvertSourceLanguage(SHADER_ORIGIN origin);
		const char* GetExtensionFromOrigin(SHADER_ORIGIN origin);
		SlangCompileTarget ConvertTarget(GRAPHICS_API api, i32 majorVer, i32 minorVer);
		SlangProfileID ConvertProfile(slang::IGlobalSession* globalSession, GRAPHICS_API api, i32 majorVer, i32 minorVer);

		ShaderStageFlags ConvertSlangStageToFlags(SlangStage stage);
		BASE_FORMAT ConvertScalarTypeToBaseFormat(slang::TypeReflection::ScalarType scalarType, u32 vectorSize);
	}
}
