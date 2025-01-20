
#include "shader_type_converter.h"

#include "sanity.h"
#include "logger.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		STATIC_ASSERT(static_cast<u32>(SHADER_KIND::MAX) == 4);
		EShLanguage ConvertShaderKind(SHADER_KIND kind)
		{
			switch (kind)
			{
			case SHADER_KIND::VERTEX: return EShLangVertex;
			case SHADER_KIND::GEOMETRY: return EShLangGeometry;
			case SHADER_KIND::FRAGMENT: return EShLangFragment;
			case SHADER_KIND::COMPUTE: return EShLangCompute;
			}

			LogError("Failed to convert shader kind. SHADER_KIND::MAX is not a valid value!");
			return {};
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_OPTIMIZATION_LEVEL::MAX) == 3);
		EShOptimizationLevel ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level)
		{
			switch (level)
			{
			case SHADER_OPTIMIZATION_LEVEL::NONE: return EShOptNone;
			case SHADER_OPTIMIZATION_LEVEL::O1: return EShOptSimple;
			case SHADER_OPTIMIZATION_LEVEL::O2: return EShOptFull;
			}

			LogError("Failed to convert shader optimization level. SHADER_OPTIMIZATION_LEVEL::MAX is not a valid value!");
			return {};
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_ORIGIN::MAX) == 2);
		glslang::EShSource ConvertSourceLanguage(SHADER_ORIGIN origin)
		{
			switch (origin)
			{
			case SHADER_ORIGIN::GLSL: return glslang::EShSourceGlsl;
			case SHADER_ORIGIN::HLSL: return glslang::EShSourceHlsl;
			}

			LogError("Failed to convert shader optimization level. SHADER_ORIGIN::MAX is not a valid value!");
			return {};
		}
	}
}