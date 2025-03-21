
#include "glslang_type_converter.h"

#include "sanity.h"
#include "logger.h"

namespace PHX
{
	namespace GLSLANG_UTILS
	{
		STATIC_ASSERT(static_cast<u32>(SHADER_STAGE::MAX) == 4);
		EShLanguage ConvertShaderStage(SHADER_STAGE kind)
		{
			switch (kind)
			{
			case SHADER_STAGE::VERTEX:   return EShLangVertex;
			case SHADER_STAGE::GEOMETRY: return EShLangGeometry;
			case SHADER_STAGE::FRAGMENT: return EShLangFragment;
			case SHADER_STAGE::COMPUTE:  return EShLangCompute;
			}

			LogError("Failed to convert shader kind. SHADER_KIND::MAX is not a valid value!");
			return {};
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_OPTIMIZATION_LEVEL::MAX) == 4);
		EShOptimizationLevel ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level)
		{
			switch (level)
			{
			case SHADER_OPTIMIZATION_LEVEL::NONE:             return EShOptNone;
			case SHADER_OPTIMIZATION_LEVEL::PERFORMANCE_FAST: return EShOptSimple;
			case SHADER_OPTIMIZATION_LEVEL::PERFORMANCE_FULL: return EShOptFull;
			case SHADER_OPTIMIZATION_LEVEL::SIZE:             return EShOptNone; // This enum value is used separately
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