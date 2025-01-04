
#include "shader_type_converter.h"

#include "sanity.h"
#include "logger.h"

namespace PHX
{
	namespace SHADER_UTILS
	{
		STATIC_ASSERT(static_cast<u32>(SHADER_KIND::MAX) == 4);
		shaderc_shader_kind ConvertShaderKind(SHADER_KIND kind)
		{
			switch (kind)
			{
			case SHADER_KIND::VERTEX: return shaderc_vertex_shader;
			case SHADER_KIND::GEOMETRY: return shaderc_geometry_shader;
			case SHADER_KIND::FRAGMENT: return shaderc_fragment_shader;
			case SHADER_KIND::COMPUTE: return shaderc_compute_shader;
			}

			LogError("Failed to convert shader kind. SHADER_KIND::MAX is not a valid value!");
			return {};
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_OPTIMIZATION_LEVEL::MAX) == 3);
		shaderc_optimization_level ConvertOptimizationLevel(SHADER_OPTIMIZATION_LEVEL level)
		{
			switch (level)
			{
			case SHADER_OPTIMIZATION_LEVEL::NONE: return shaderc_optimization_level_zero;
			case SHADER_OPTIMIZATION_LEVEL::SIZE: return shaderc_optimization_level_size;
			case SHADER_OPTIMIZATION_LEVEL::PERFORMANCE: return shaderc_optimization_level_performance;
			}

			LogError("Failed to convert shader optimization level. SHADER_OPTIMIZATION_LEVEL::MAX is not a valid value!");
			return {};
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_ORIGIN::MAX) == 2);
		shaderc_source_language ConvertSourceLanguage(SHADER_ORIGIN origin)
		{
			switch (origin)
			{
			case SHADER_ORIGIN::GLSL: return shaderc_source_language_glsl;
			case SHADER_ORIGIN::HLSL: return shaderc_source_language_hlsl;
			}

			LogError("Failed to convert shader optimization level. SHADER_ORIGIN::MAX is not a valid value!");
			return {};
		}
	}
}