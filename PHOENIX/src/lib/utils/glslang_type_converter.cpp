
#include "glslang_type_converter.h"

#include "core/global_settings.h"
#include "sanity.h"
#include "logger.h"

namespace PHX
{
	namespace GLSLANG_UTILS
	{
		STATIC_ASSERT(static_cast<u8>(SHADER_STAGE::MAX) == 10);
		EShLanguage ConvertShaderStage(SHADER_STAGE kind)
		{
			switch (kind)
			{
			case SHADER_STAGE::VERTEX:       return EShLangVertex;
			case SHADER_STAGE::GEOMETRY:     return EShLangGeometry;
			case SHADER_STAGE::FRAGMENT:     return EShLangFragment;
			case SHADER_STAGE::COMPUTE:      return EShLangCompute;
			case SHADER_STAGE::RAYGEN:       return EShLangRayGen;
			case SHADER_STAGE::INTERSECTION: return EShLangIntersect;
			case SHADER_STAGE::ANY_HIT:      return EShLangAnyHit;
			case SHADER_STAGE::CLOSEST_HIT:  return EShLangClosestHit;
			case SHADER_STAGE::MISS:         return EShLangMiss;
			case SHADER_STAGE::CALLABLE:     return EShLangCallable;
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
					switch (currBitFlag)
					{
					case EShLangVertexMask:         { flagResult |= SHADER_STAGE_FLAG_VERTEX; break; }
					case EShLangTessControlMask:    { TODO(); break; }
					case EShLangTessEvaluationMask: { TODO(); break; }
					case EShLangGeometryMask:       { flagResult |= SHADER_STAGE_FLAG_GEOMETRY; break; }
					case EShLangFragmentMask:       { flagResult |= SHADER_STAGE_FLAG_FRAGMENT; break; }
					case EShLangComputeMask:        { flagResult |= SHADER_STAGE_FLAG_COMPUTE; break; }
					case EShLangRayGenMask:         { flagResult |= SHADER_STAGE_FLAG_RAYGEN; break; }
					case EShLangIntersectMask:      { flagResult |= SHADER_STAGE_FLAG_INTERSECTION; break; }
					case EShLangAnyHitMask:         { flagResult |= SHADER_STAGE_FLAG_ANY_HIT; break; }
					case EShLangClosestHitMask:     { flagResult |= SHADER_STAGE_FLAG_CLOSEST_HIT; break; }
					case EShLangMissMask:           { flagResult |= SHADER_STAGE_FLAG_MISS; break; }
					case EShLangCallableMask:       { flagResult |= SHADER_STAGE_FLAG_CALLABLE; break; }
					case EShLangTaskMask:           { TODO(); break; }
					case EShLangMeshMask:           { TODO(); break; }
					}
				}
			}

			return flagResult;
		}

		BASE_FORMAT ConvertIOTypeToBaseFormat(glslang::TBasicType type, u32 vectorSize)
		{
			switch (type)
			{
			case glslang::EbtFloat:
			{
				switch (vectorSize)
				{
				case 1: return BASE_FORMAT::R32_FLOAT;
				case 2: return BASE_FORMAT::R32G32_FLOAT;
				case 3: return BASE_FORMAT::R32G32B32_FLOAT;
				case 4: return BASE_FORMAT::R32G32B32A32_FLOAT;
				default: break;
				}

				break;
			}
			case glslang::EbtInt:
			{
				switch (vectorSize)
				{
				case 1: return BASE_FORMAT::R32_SINT;
				case 2: return BASE_FORMAT::R32G32_SINT;
				case 3: return BASE_FORMAT::R32G32B32_SINT;
				case 4: return BASE_FORMAT::R32G32B32A32_SINT;
				default: break;
				}

				break;
			}
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert IO type to base format. Unhandled type or vector size!");
			return BASE_FORMAT::INVALID;
		}

		glslang::EShTargetClientVersion GetClientVersion()
		{
			const Settings& settings = GlobalSettings::Get().GetSettings();
			const GRAPHICS_API& api = settings.backendAPI;
			const i32& majorVer = settings.backendAPIMajorVersion;
			const i32& minorVer = settings.backendAPIMinorVersion;

			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				if (majorVer == 1 && minorVer == 0)
				{
					return glslang::EShTargetClientVersion::EShTargetVulkan_1_0;
				}
				else if (majorVer == 1 && minorVer == 1)
				{
					return glslang::EShTargetClientVersion::EShTargetVulkan_1_1;
				}
				else if (majorVer == 1 && minorVer == 2)
				{
					return glslang::EShTargetClientVersion::EShTargetVulkan_1_2;
				}
				else if (majorVer == 1 && minorVer == 3)
				{
					return glslang::EShTargetClientVersion::EShTargetVulkan_1_3;
				}

				// No match found
				break;
			}
			/*
			case GRAPHICS_API::OPENGL
			{
				return glslang::EShTargetClientVersion::EShTargetOpenGL_450;
			}
			*/
			default:
			{
				TODO();
				break;
			}
			}

			return glslang::EShTargetClientVersion::EShTargetClientVersionCount;
		}

		glslang::EShClient GetClient()
		{
			const Settings& settings = GlobalSettings::Get().GetSettings();
			const GRAPHICS_API& api = settings.backendAPI;

			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				return glslang::EShClient::EShClientVulkan;
			}
			/*
			case GRAPHICS_API::OPENGL
			{
				return glslang::EShClient::EShClientOpenGL;
			}
			*/
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to get glslang EShClient. Unsupported backendAPI!");
			return glslang::EShClient::EShClientNone;
		}
	}
}