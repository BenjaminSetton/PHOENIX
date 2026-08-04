
#include <slang.h>
#include <slang-com-ptr.h>
#include <string>
#include <vector>

#include "PHX/phx.h"

#include "BSL/crc32.h"
#include "BSL/deferred_caller.h"
#include "BSL/logger.h"
#include "BSL/sanity.h"
#include "core/core_object_manager.h"
#include "core/global_settings.h"
#include "utils/slang_type_converter.h"

using namespace BSL;

namespace PHX
{
	static constexpr u32 VER_MAJOR_SIZE = 8;  // 256
	static constexpr u32 VER_MINOR_SIZE = 8;  // 256
	static constexpr u32 VER_PATCH_SIZE = 16; // 65536

	// PHOENIX VERSION 0.1.0
	static constexpr u32 VER_MAJOR = 0;
	static constexpr u32 VER_MINOR = 1;
	static constexpr u32 VER_PATCH = 0;

	#define BUILD_VERSION(major, minor, patch) \
		(major << (VER_MINOR_SIZE + VER_PATCH_SIZE)) | \
		(minor << (VER_PATCH_SIZE)				   ) | \
		(patch << 0								   )

	static constexpr const char* SHADER_STAGE_NAMES[static_cast<u32>(SHADER_STAGE::MAX)] =
	{
		"VERTEX",
		"GEOMETRY",
		"FRAGMENT",
		"COMPUTE",
		"RAYGEN",
		"INTERSECTION",
		"ANY_HIT",
		"CLOSEST_HIT",
		"MISS",
		"CALLABLE",
		"TESSELLATION_CONTROL",
		"TESSELLATION_EVALUATION",
	};

	// Lazy-init the global Slang session
	static Slang::ComPtr<slang::IGlobalSession> g_slangGlobalSession;
	static slang::IGlobalSession* GetSlangGlobalSession()
	{
		if (g_slangGlobalSession == nullptr)
		{
			SlangGlobalSessionDesc desc{};
			desc.enableGLSL = true;
			SlangResult result = slang_createGlobalSession2(&desc, g_slangGlobalSession.writeRef());
			if (SLANG_FAILED(result) || g_slangGlobalSession == nullptr)
			{
				LogError("Failed to create Slang global session!");
				return nullptr;
			}
		}
		return g_slangGlobalSession;
	}

	static bool CheckMandatorySettings(const PHX::Settings& settings)
	{
		bool isValid = true;

		// backendAPI setting has no fallback enum value

		if (settings.backendAPIMajorVersion == -1)
		{
			LogError("Mandatory setting \"backendAPIMajorVersion\" has not been set!");
			isValid = false;
		}

		if (settings.backendAPIMinorVersion == -1)
		{
			LogError("Mandatory setting \"backendAPIMinorVersion\" has not been set!");
			isValid = false;
		}

		if (settings.swapChainOutdatedCallback == nullptr)
		{
			LogError("Mandatory setting \"swapChainOutdatedCallback\" has not been set!");
			isValid = false;
		}

		if (settings.windowFocusChangedCallback == nullptr)
		{
			LogError("Mandatory setting \"windowFocusChangedCallback\" has not been set!");
			isValid = false;
		}

		if (settings.windowMaximizedCallback == nullptr)
		{
			LogError("Mandatory setting \"windowMaximizedCallback\" has not been set!");
			isValid = false;
		}

		if (settings.windowMinimizedCallback == nullptr)
		{
			LogError("Mandatory setting \"windowMinimizedCallback\" has not been set!");
			isValid = false;
		}

		if (settings.windowResizedCallback == nullptr)
		{
			LogError("Mandatory setting \"windowResizedCallback\" has not been set!");
			isValid = false;
		}

		return isValid;
	}

	STATUS_CODE Initialize(const Settings& initSettings, WindowHandle window)
	{
		if (!window.IsValid())
		{
			LogError("Failed to initialize library. Window handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		if (!CheckMandatorySettings(initSettings))
		{
			// Errors are logged in the check function specifically for what's missing
			LogError("Failed to initialize library! One or more mandatory settings have not been set");
			return STATUS_CODE::ERR_API;
		}

		// Only this function should ever set the settings!
		GlobalSettings::Get().SetSettings(initSettings);

		// Initialize core graphics objects
		STATUS_CODE res = CoreObjectManager::Get().CreateCoreObjects(window);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE Update(float deltaTime)
	{
		UNUSED(deltaTime);

		DeferredCaller& deferredCaller = CoreObjectManager::Get().GetDeferredCaller();
		deferredCaller.Update();

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE Shutdown()
	{
		DeferredCaller& deferredCaller = CoreObjectManager::Get().GetDeferredCaller();
		deferredCaller.Flush();

		CoreObjectManager::Get().Shutdown();
		return STATUS_CODE::SUCCESS;
	}

	u32 GetFullVersion()
	{
		return BUILD_VERSION(VER_MAJOR, VER_MINOR, VER_PATCH);
	}

	u32 GetMajorVersion()
	{
		return VER_MAJOR;
	}

	u32 GetMinorVersion()
	{
		return VER_MINOR;
	}

	u32 GetPatchVersion()
	{
		return VER_PATCH;
	}

	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, WindowHandle& window)
	{
		return CoreObjectManager::Get().CreateWindow(createInfo, window);
	}

	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, RenderDeviceHandle& renderDevice)
	{
		return CoreObjectManager::Get().CreateRenderDevice(createInfo, renderDevice);
	}

	STATUS_CODE CompileShader(const ShaderSourceData& srcData, CompiledShader& out_result)
	{
		const char* shaderStageStr = SHADER_STAGE_NAMES[static_cast<u32>(srcData.stage)];
		if (srcData.data == nullptr)
		{
			LogError("Failed to compile %s shader! Source data's data pointer is null", shaderStageStr);
			return STATUS_CODE::ERR_API;
		}

		if (srcData.entryPoint == nullptr)
		{
			LogError("Failed to compile %s shader! Entry point is null", shaderStageStr);
			return STATUS_CODE::ERR_API;
		}

		const Settings& settings = GlobalSettings::Get().GetSettings();

		// Get or lazily create the global Slang session
		slang::IGlobalSession* globalSession = GetSlangGlobalSession();
		if (globalSession == nullptr)
		{
			LogError("Failed to get Slang global session for %s shader!", shaderStageStr);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Determine target and profile based on the backend API
		SlangCompileTarget target = SLANG_UTILS::ConvertTarget(settings.backendAPI, settings.backendAPIMajorVersion, settings.backendAPIMinorVersion);
		SlangProfileID profile = SLANG_UTILS::ConvertProfile(globalSession, settings.backendAPI, settings.backendAPIMajorVersion, settings.backendAPIMinorVersion);

		// Configure the target
		slang::TargetDesc targetDesc{};
		targetDesc.format = target;
		targetDesc.profile = profile;

		// Slang's native SPIR-V emitter has incomplete target-intrinsic coverage for raw GLSL-origin
		// source (e.g. basic operators like '*' are only defined for the textual "glsl" target case,
		// not the direct SPIR-V case), which crashes slang-emit-spirv.cpp. Fall back to the mature
		// via-glslang backend for GLSL-origin shaders to avoid this.
		if (srcData.origin == SHADER_ORIGIN::GLSL)
		{
			targetDesc.flags &= ~SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;
		}

		// Collect search paths for include resolution
		std::vector<const char*> searchPaths;
		for (u32 i = 0; i < srcData.includePathCount; i++)
		{
			searchPaths.push_back(srcData.includePaths[i]);
		}

		// Disable extra debug info (OpLine, local variable debug info, etc). Note this does NOT
		// remove OpSource; that's filtered out as a known false-positive in the Vulkan debug
		// messenger callback instead (see OnValidationMessageReceived in core_vk.cpp).
		slang::CompilerOptionEntry noDebugInfo{};
		noDebugInfo.name = slang::CompilerOptionName::DebugInformation;
		noDebugInfo.value.kind = slang::CompilerOptionValueKind::Int;
		noDebugInfo.value.intValue0 = SLANG_DEBUG_INFO_LEVEL_NONE;

		// Create session description
		slang::SessionDesc sessionDesc{};
		sessionDesc.targets = &targetDesc;
		sessionDesc.targetCount = 1;
		sessionDesc.searchPaths = searchPaths.data();
		sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());
		sessionDesc.compilerOptionEntries = &noDebugInfo;
		sessionDesc.compilerOptionEntryCount = 1;

		// Create a Slang session
		Slang::ComPtr<slang::ISession> session;
		SlangResult result = globalSession->createSession(sessionDesc, session.writeRef());
		if (SLANG_FAILED(result) || session == nullptr)
		{
			LogError("Failed to create Slang session for %s shader!", shaderStageStr);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Determine the source language file extension for Slang to detect the language
		const char* sourceExt = SLANG_UTILS::GetExtensionFromOrigin(srcData.origin);
		std::string sourcePath = std::string("phx_shader") + sourceExt;

		// Load the shader source as a module
		Slang::ComPtr<slang::IBlob> diagnostics;
		slang::IModule* module = session->loadModuleFromSourceString("phx_shader", sourcePath.c_str(), srcData.data, diagnostics.writeRef());
		if (module == nullptr)
		{
			const char* diagMsg = (diagnostics != nullptr) ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown error";
			LogError("Failed to load %s shader module! Got error: \"%s\"", shaderStageStr, diagMsg);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Find and check the entry point
		SlangStage slangStage = SLANG_UTILS::ConvertShaderStage(srcData.stage);
		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		diagnostics = nullptr;
		result = module->findAndCheckEntryPoint(srcData.entryPoint, slangStage, entryPoint.writeRef(), diagnostics.writeRef());
		if (SLANG_FAILED(result) || entryPoint == nullptr)
		{
			const char* diagMsg = (diagnostics != nullptr) ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown error";
			LogError("Failed to find entry point \"%s\" in %s shader! Got error: \"%s\"", srcData.entryPoint, shaderStageStr, diagMsg);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Compose the module and entry point into a component type
		static constexpr u32 COMPONENT_COUNT = 2;
		Slang::ComPtr<slang::IComponentType> composedProgram;
		slang::IComponentType* components[COMPONENT_COUNT] = { module, entryPoint };
		diagnostics = nullptr;
		result = session->createCompositeComponentType(components, COMPONENT_COUNT, composedProgram.writeRef(), diagnostics.writeRef());
		if (SLANG_FAILED(result) || composedProgram == nullptr)
		{
			const char* diagMsg = (diagnostics != nullptr) ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown error";
			LogError("Failed to compose %s shader program! Got error: \"%s\"", shaderStageStr, diagMsg);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Get compiled shader code
		Slang::ComPtr<slang::IBlob> compiledCode;
		diagnostics = nullptr;
		result = composedProgram->getEntryPointCode(0, 0, compiledCode.writeRef(), diagnostics.writeRef());
		if (SLANG_FAILED(result) || compiledCode == nullptr)
		{
			const char* diagMsg = (diagnostics != nullptr) ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown error";
			LogError("Failed to compile %s shader to target code! Got error: \"%s\"", shaderStageStr, diagMsg);
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Copy compiled bytecode into the output struct
		size_t codeSize = compiledCode->getBufferSize();
		u32 numWords = static_cast<u32>(codeSize / sizeof(u32));
		out_result.data = std::shared_ptr<u32[]>(new u32[numWords]);
		out_result.size = numWords;
		memcpy(out_result.data.get(), compiledCode->getBufferPointer(), codeSize);

		// Optionally perform reflection
		if (srcData.performReflection)
		{
			slang::ProgramLayout* programLayout = composedProgram->getLayout(0, diagnostics.writeRef());
			if (programLayout == nullptr)
			{
				const char* diagMsg = (diagnostics != nullptr) ? static_cast<const char*>(diagnostics->getBufferPointer()) : "Unknown error";
				LogError("Failed to get program layout for %s shader! Got error: \"%s\"", shaderStageStr, diagMsg);
				return STATUS_CODE::ERR_INTERNAL;
			}

			// UNIFORMS (global shader parameters)
			{
				u32 paramCount = programLayout->getParameterCount();
				if (paramCount > 0)
				{
					out_result.reflectionData.uniforms = std::shared_ptr<ShaderUniformData[]>(new ShaderUniformData[paramCount]);
					out_result.reflectionData.uniformCount = paramCount;

					for (u32 i = 0; i < paramCount; i++)
					{
						slang::VariableLayoutReflection* param = programLayout->getParameterByIndex(i);
						ShaderUniformData& uniformData = out_result.reflectionData.uniforms[i];
						uniformData.name = param->getName();
						uniformData.binding = param->getBindingIndex();
						uniformData.size = static_cast<u32>(param->getTypeLayout()->getSize());
						uniformData.stages = 0; // Global parameters are accessible from all stages
						uniformData.offset = static_cast<u32>(param->getOffset());
					}
				}
			}

			// LOCAL SIZE (compute shaders only)
			{
				if (srcData.stage == SHADER_STAGE::COMPUTE)
				{
					SlangUInt entryPointCount = programLayout->getEntryPointCount();
					if (entryPointCount > 0)
					{
						slang::EntryPointReflection* entryPointReflection = programLayout->getEntryPointByIndex(0);
						static constexpr u32 GROUP_SIZE_AXIS_COUNT = 3;
						SlangUInt groupSize[GROUP_SIZE_AXIS_COUNT] = { 1, 1, 1 };
						entryPointReflection->getComputeThreadGroupSize(GROUP_SIZE_AXIS_COUNT, groupSize);
						out_result.reflectionData.localSize.SetX(static_cast<u32>(groupSize[0]));
						out_result.reflectionData.localSize.SetY(static_cast<u32>(groupSize[1]));
						out_result.reflectionData.localSize.SetZ(static_cast<u32>(groupSize[2]));
					}
				}
			}

			// INPUTS and OUTPUTS (entry point parameters)
			{
				SlangUInt entryPointCount = programLayout->getEntryPointCount();
				if (entryPointCount > 0)
				{
					slang::EntryPointReflection* entryPointReflection = programLayout->getEntryPointByIndex(0);
					u32 epParamCount = entryPointReflection->getParameterCount();

					// Native Slang shaders group per-stage varying I/O into a struct (e.g. VSInput/VSOutput)
					// instead of exposing loose top-level parameters like GLSL did. A struct member only
					// occupies a real user-facing location if its own category still matches the parent's
					// category; members bound to system-value semantics (e.g. SV_Position, SV_Target) report
					// a different category and are skipped, since they don't consume a location.
					auto isVaryingIOField = [](slang::VariableLayoutReflection* field, slang::ParameterCategory category)
					{
						return field->getCategory() == category;
					};

					// Returns true if the parameter is a per-vertex varying input/output. Patch-level
					// inputs in tessellation shaders (e.g. InputPatch/OutputPatch) report VaryingInput
					// category but a non-varying type kind, so they are excluded here.
					auto isVaryingIOParameter = [](slang::TypeLayoutReflection* typeLayout)
					{
						slang::ParameterCategory category = typeLayout->getParameterCategory();
						if (category != slang::ParameterCategory::VaryingInput &&
							category != slang::ParameterCategory::VaryingOutput)
						{
							return false;
						}

						slang::TypeReflection::Kind kind = typeLayout->getKind();
						return kind == slang::TypeReflection::Kind::Struct ||
							kind == slang::TypeReflection::Kind::Scalar ||
							kind == slang::TypeReflection::Kind::Vector ||
							kind == slang::TypeReflection::Kind::Matrix;
					};

					// Count inputs and outputs
					u32 inputCount = 0;
					u32 outputCount = 0;
					for (u32 i = 0; i < epParamCount; i++)
					{
						slang::VariableLayoutReflection* param = entryPointReflection->getParameterByIndex(i);
						slang::TypeLayoutReflection* typeLayout = param->getTypeLayout();
						slang::ParameterCategory category = typeLayout->getParameterCategory();

						if (!isVaryingIOParameter(typeLayout))
						{
							continue;
						}

						if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
						{
							u32 fieldCount = typeLayout->getFieldCount();
							for (u32 f = 0; f < fieldCount; f++)
							{
								if (isVaryingIOField(typeLayout->getFieldByIndex(f), category))
								{
									(category == slang::ParameterCategory::VaryingInput) ? inputCount++ : outputCount++;
								}
							}
						}
						else
						{
							(category == slang::ParameterCategory::VaryingInput) ? inputCount++ : outputCount++;
						}
					}

					// Allocate the IO arrays
					if (inputCount > 0)
					{
						out_result.reflectionData.inputCount = inputCount;
						out_result.reflectionData.inputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[inputCount]);
					}
					if (outputCount > 0)
					{
						out_result.reflectionData.outputCount = outputCount;
						out_result.reflectionData.outputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[outputCount]);
					}

					// Builds a ShaderIOData entry from a variable layout using the given category for its offset
					auto makeIOData = [](slang::VariableLayoutReflection* var, slang::ParameterCategory category)
					{
						ShaderIOData ioData;
						ioData.name = var->getName();
						ioData.location = static_cast<u32>(var->getOffset(category));
						ioData.binding = var->getBindingIndex();

						slang::TypeReflection* type = var->getTypeLayout()->getType();
						u32 vectorSize = type->getColumnCount();
						if (vectorSize == 0) vectorSize = 1;
						ioData.format = SLANG_UTILS::ConvertScalarTypeToBaseFormat(type->getScalarType(), vectorSize);

						return ioData;
					};

					// Fill arrays
					u32 inputIdx = 0;
					u32 outputIdx = 0;
					for (u32 i = 0; i < epParamCount; i++)
					{
						slang::VariableLayoutReflection* param = entryPointReflection->getParameterByIndex(i);
						slang::TypeLayoutReflection* typeLayout = param->getTypeLayout();
						slang::ParameterCategory category = typeLayout->getParameterCategory();

						if (!isVaryingIOParameter(typeLayout))
						{
							continue;
						}

						if (typeLayout->getKind() == slang::TypeReflection::Kind::Struct)
						{
							u32 fieldCount = typeLayout->getFieldCount();
							for (u32 f = 0; f < fieldCount; f++)
							{
								slang::VariableLayoutReflection* field = typeLayout->getFieldByIndex(f);
								if (!isVaryingIOField(field, category))
								{
									continue;
								}

								ShaderIOData ioData = makeIOData(field, category);
								if (category == slang::ParameterCategory::VaryingInput)
								{
									out_result.reflectionData.inputs[inputIdx++] = ioData;
								}
								else
								{
									out_result.reflectionData.outputs[outputIdx++] = ioData;
								}
							}
						}
						else
						{
							ShaderIOData ioData = makeIOData(param, category);
							if (category == slang::ParameterCategory::VaryingInput)
							{
								out_result.reflectionData.inputs[inputIdx++] = ioData;
							}
							else
							{
								out_result.reflectionData.outputs[outputIdx++] = ioData;
							}
						}
					}
				}
			}

			out_result.reflectionData.isValid = true;
		}

		return STATUS_CODE::SUCCESS;
	}
}