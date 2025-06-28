
#include "glslang_type_converter.h"

#include "sanity.h"
#include "logger.h"

namespace PHX
{
	namespace GLSLANG_UTILS
	{
		STATIC_ASSERT(static_cast<u8>(SHADER_STAGE::MAX) == 4);
		EShLanguage ConvertShaderStage(SHADER_STAGE kind)
		{
			switch (kind)
			{
			case SHADER_STAGE::VERTEX:   return EShLangVertex;
			case SHADER_STAGE::GEOMETRY: return EShLangGeometry;
			case SHADER_STAGE::FRAGMENT: return EShLangFragment;
			case SHADER_STAGE::COMPUTE:  return EShLangCompute;
			}

			LogError("Failed to convert shader kind. SHADER_KIND::COUNT is not a valid value!");
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

		ShaderStageFlags ConvertShaderStageFlags(EShLanguageMask stageMask)
		{
			ShaderStageFlags flagResult = 0;

			u32 maskBitSize = sizeof(EShLanguageMask) * 8;
			for (u32 i = 0; i < maskBitSize; i++)
			{
				u32 currBitFlag = (1 << i);
				if ((currBitFlag & stageMask) != 0)
				{
					switch (maskBitSize)
					{
					case EShLangVertexMask:         { flagResult |= SHADER_STAGE_FLAG_VERTEX; break; }
					case EShLangTessControlMask:    { TODO(); break; }
					case EShLangTessEvaluationMask: { TODO(); break; }
					case EShLangGeometryMask:       { flagResult |= SHADER_STAGE_FLAG_GEOMETRY; break; }
					case EShLangFragmentMask:       { flagResult |= SHADER_STAGE_FLAG_FRAGMENT; break; }
					case EShLangComputeMask:        { flagResult |= SHADER_STAGE_FLAG_COMPUTE; break; }
					case EShLangRayGenMask:         { TODO(); break; }
					case EShLangIntersectMask:      { TODO(); break; }
					case EShLangAnyHitMask:         { TODO(); break; }
					case EShLangClosestHitMask:     { TODO(); break; }
					case EShLangMissMask:           { TODO(); break; }
					case EShLangCallableMask:       { TODO(); break; }
					case EShLangTaskMask:           { TODO(); break; }
					case EShLangMeshMask:           { TODO(); break; }
					}
				}
			}

			return flagResult;
		}
	}
}