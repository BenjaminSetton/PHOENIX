
#include "slang_type_converter.h"

#include "BSL/sanity.h"
#include "BSL/logger.h"
#include "core/global_settings.h"

using namespace BSL;

namespace PHX
{
	namespace SLANG_UTILS
	{
		STATIC_ASSERT(static_cast<u8>(SHADER_STAGE::MAX) == 12);
		SlangStage ConvertShaderStage(SHADER_STAGE kind)
		{
			switch (kind)
			{
			case SHADER_STAGE::VERTEX:                  return SLANG_STAGE_VERTEX;
			case SHADER_STAGE::GEOMETRY:                return SLANG_STAGE_GEOMETRY;
			case SHADER_STAGE::FRAGMENT:                return SLANG_STAGE_FRAGMENT;
			case SHADER_STAGE::COMPUTE:                 return SLANG_STAGE_COMPUTE;
			case SHADER_STAGE::RAYGEN:                  return SLANG_STAGE_RAY_GENERATION;
			case SHADER_STAGE::INTERSECTION:            return SLANG_STAGE_INTERSECTION;
			case SHADER_STAGE::ANY_HIT:                 return SLANG_STAGE_ANY_HIT;
			case SHADER_STAGE::CLOSEST_HIT:             return SLANG_STAGE_CLOSEST_HIT;
			case SHADER_STAGE::MISS:                    return SLANG_STAGE_MISS;
			case SHADER_STAGE::CALLABLE:                return SLANG_STAGE_CALLABLE;
			case SHADER_STAGE::TESSELLATION_CONTROL:    return SLANG_STAGE_HULL;
			case SHADER_STAGE::TESSELLATION_EVALUATION: return SLANG_STAGE_DOMAIN;
			}

			LogError("Failed to convert shader stage. SHADER_STAGE::MAX is not a valid value!");
			return SLANG_STAGE_NONE;
		}

		STATIC_ASSERT(static_cast<u32>(SHADER_ORIGIN::MAX) == 3);
		SlangSourceLanguage ConvertSourceLanguage(SHADER_ORIGIN origin)
		{
			switch (origin)
			{
			case SHADER_ORIGIN::GLSL:  return SLANG_SOURCE_LANGUAGE_GLSL;
			case SHADER_ORIGIN::HLSL:  return SLANG_SOURCE_LANGUAGE_HLSL;
			case SHADER_ORIGIN::SLANG: return SLANG_SOURCE_LANGUAGE_SLANG;
			}

			LogError("Failed to convert shader origin. SHADER_ORIGIN::MAX is not a valid value!");
			return SLANG_SOURCE_LANGUAGE_UNKNOWN;
		}

		const char* GetExtensionFromOrigin(SHADER_ORIGIN origin)
		{
			switch (origin)
			{
			case SHADER_ORIGIN::GLSL:  return ".glsl";
			case SHADER_ORIGIN::HLSL:  return ".hlsl";
			case SHADER_ORIGIN::SLANG: return ".slang";
			}

			LogError("Failed to get extension from shader origin. SHADER_ORIGIN::MAX is not a valid value!");
			return ".slang";
		}

		SlangCompileTarget ConvertTarget(GRAPHICS_API api, i32 majorVer, i32 minorVer)
		{
			UNUSED(majorVer);
			UNUSED(minorVer);

			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				return SLANG_SPIRV;
			}
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert Slang compile target. Unsupported graphics API!");
			return SLANG_TARGET_UNKNOWN;
		}

		SlangProfileID ConvertProfile(slang::IGlobalSession* globalSession, GRAPHICS_API api, i32 majorVer, i32 minorVer)
		{
			UNUSED(majorVer);
			UNUSED(minorVer);

			switch (api)
			{
			case GRAPHICS_API::VULKAN:
			{
				return globalSession->findProfile("glsl_450");
			}
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert Slang profile. Unsupported backend API!");
			return SLANG_PROFILE_UNKNOWN;
		}

		ShaderStageFlags ConvertSlangStageToFlags(SlangStage stage)
		{
			switch (stage)
			{
			case SLANG_STAGE_VERTEX:           return SHADER_STAGE_FLAG_VERTEX;
			case SLANG_STAGE_HULL:             return SHADER_STAGE_FLAG_TESSELLATION_CONTROL;
			case SLANG_STAGE_DOMAIN:           return SHADER_STAGE_FLAG_TESSELLATION_EVALUATION;
			case SLANG_STAGE_GEOMETRY:         return SHADER_STAGE_FLAG_GEOMETRY;
			case SLANG_STAGE_FRAGMENT:         return SHADER_STAGE_FLAG_FRAGMENT;
			case SLANG_STAGE_COMPUTE:          return SHADER_STAGE_FLAG_COMPUTE;
			case SLANG_STAGE_RAY_GENERATION:   return SHADER_STAGE_FLAG_RAYGEN;
			case SLANG_STAGE_INTERSECTION:     return SHADER_STAGE_FLAG_INTERSECTION;
			case SLANG_STAGE_ANY_HIT:          return SHADER_STAGE_FLAG_ANY_HIT;
			case SLANG_STAGE_CLOSEST_HIT:      return SHADER_STAGE_FLAG_CLOSEST_HIT;
			case SLANG_STAGE_MISS:             return SHADER_STAGE_FLAG_MISS;
			case SLANG_STAGE_CALLABLE:         return SHADER_STAGE_FLAG_CALLABLE;
			default:                           break;
			}

			ASSERT_ALWAYS("Failed to convert Slang stage to shader stage flags. Unhandled stage!");
			return 0;
		}

		BASE_FORMAT ConvertScalarTypeToBaseFormat(slang::TypeReflection::ScalarType scalarType, u32 vectorSize)
		{
			switch (scalarType)
			{
			case slang::TypeReflection::ScalarType::Float32:
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
			case slang::TypeReflection::ScalarType::Int32:
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
			case slang::TypeReflection::ScalarType::UInt32:
			{
				switch (vectorSize)
				{
				case 1: return BASE_FORMAT::R32_UINT;
				case 2: return BASE_FORMAT::R32G32_UINT;
				case 3: return BASE_FORMAT::R32G32B32_UINT;
				case 4: return BASE_FORMAT::R32G32B32A32_UINT;
				default: break;
				}
				break;
			}
			default:
			{
				break;
			}
			}

			ASSERT_ALWAYS("Failed to convert scalar type to base format. Unhandled type or vector size!");
			return BASE_FORMAT::INVALID;
		}
	}
}
