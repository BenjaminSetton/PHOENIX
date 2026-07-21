
#include <glslang/Include/Types.h>

#include <SPIRV/GlslangToSpv.h>

#include "PHX/phx.h"

#include "core/core_object_manager.h"
#include "core/global_settings.h"
#include "utils/crc32.h"
#include "utils/deferred_caller.h"
#include "utils/glslang_includer.h"
#include "utils/glslang_type_converter.h"
#include "utils/logger.h"
#include "utils/sanity.h"

namespace PHX
{

	static constexpr u32 VER_MAJOR_SIZE = 8; // 256
	static constexpr u32 VER_MINOR_SIZE = 8; // 256
	static constexpr u32 VER_PATCH_SIZE = 16; // 65536

	// PHEONIX VERSION 0.0.1
	static constexpr u32 VER_MAJOR = 0;
	static constexpr u32 VER_MINOR = 0;
	static constexpr u32 VER_PATCH = 1;

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
	};

	// TODO - Move into own shader_compiler.h or something...
	static void GetDefaultShaderResources(TBuiltInResource& resources)
	{
		resources.maxLights = 32;
		resources.maxClipPlanes = 6;
		resources.maxTextureUnits = 32;
		resources.maxTextureCoords = 32;
		resources.maxVertexAttribs = 64;
		resources.maxVertexUniformComponents = 4096;
		resources.maxVaryingFloats = 64;
		resources.maxVertexTextureImageUnits = 32;
		resources.maxCombinedTextureImageUnits = 80;
		resources.maxTextureImageUnits = 32;
		resources.maxFragmentUniformComponents = 4096;
		resources.maxDrawBuffers = 32;
		resources.maxVertexUniformVectors = 128;
		resources.maxVaryingVectors = 8;
		resources.maxFragmentUniformVectors = 16;
		resources.maxVertexOutputVectors = 16;
		resources.maxFragmentInputVectors = 15;
		resources.minProgramTexelOffset = -8;
		resources.maxProgramTexelOffset = 7;
		resources.maxClipDistances = 8;
		resources.maxComputeWorkGroupCountX = 65535;
		resources.maxComputeWorkGroupCountY = 65535;
		resources.maxComputeWorkGroupCountZ = 65535;
		resources.maxComputeWorkGroupSizeX = 1024;
		resources.maxComputeWorkGroupSizeY = 1024;
		resources.maxComputeWorkGroupSizeZ = 64;
		resources.maxComputeUniformComponents = 1024;
		resources.maxComputeTextureImageUnits = 16;
		resources.maxComputeImageUniforms = 8;
		resources.maxComputeAtomicCounters = 8;
		resources.maxComputeAtomicCounterBuffers = 1;
		resources.maxVaryingComponents = 60;
		resources.maxVertexOutputComponents = 64;
		resources.maxGeometryInputComponents = 64;
		resources.maxGeometryOutputComponents = 128;
		resources.maxFragmentInputComponents = 128;
		resources.maxImageUnits = 8;
		resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
		resources.maxImageSamples = 0;
		resources.maxVertexImageUniforms = 0;
		resources.maxTessControlImageUniforms = 0;
		resources.maxTessEvaluationImageUniforms = 0;
		resources.maxGeometryImageUniforms = 0;
		resources.maxFragmentImageUniforms = 8;
		resources.maxCombinedImageUniforms = 8;
		resources.maxGeometryTextureImageUnits = 16;
		resources.maxGeometryOutputVertices = 256;
		resources.maxGeometryTotalOutputComponents = 1024;
		resources.maxGeometryUniformComponents = 1024;
		resources.maxGeometryVaryingComponents = 64;
		resources.maxTessControlInputComponents = 128;
		resources.maxTessControlOutputComponents = 128;
		resources.maxTessControlTextureImageUnits = 16;
		resources.maxTessControlUniformComponents = 1024;
		resources.maxTessControlTotalOutputComponents = 4096;
		resources.maxTessEvaluationInputComponents = 128;
		resources.maxTessEvaluationOutputComponents = 128;
		resources.maxTessEvaluationTextureImageUnits = 16;
		resources.maxTessEvaluationUniformComponents = 1024;
		resources.maxTessPatchComponents = 120;
		resources.maxPatchVertices = 32;
		resources.maxTessGenLevel = 64;
		resources.maxViewports = 16;
		resources.maxVertexAtomicCounters = 0;
		resources.maxTessControlAtomicCounters = 0;
		resources.maxTessEvaluationAtomicCounters = 0;
		resources.maxGeometryAtomicCounters = 0;
		resources.maxFragmentAtomicCounters = 8;
		resources.maxCombinedAtomicCounters = 8;
		resources.maxAtomicCounterBindings = 1;
		resources.maxVertexAtomicCounterBuffers = 0;
		resources.maxTessControlAtomicCounterBuffers = 0;
		resources.maxTessEvaluationAtomicCounterBuffers = 0;
		resources.maxGeometryAtomicCounterBuffers = 0;
		resources.maxFragmentAtomicCounterBuffers = 1;
		resources.maxCombinedAtomicCounterBuffers = 1;
		resources.maxAtomicCounterBufferSize = 16384;
		resources.maxTransformFeedbackBuffers = 4;
		resources.maxTransformFeedbackInterleavedComponents = 64;
		resources.maxCullDistances = 8;
		resources.maxCombinedClipAndCullDistances = 8;
		resources.maxSamples = 4;

		resources.limits.nonInductiveForLoops = 1;
		resources.limits.whileLoops = 1;
		resources.limits.doWhileLoops = 1;
		resources.limits.generalUniformIndexing = 1;
		resources.limits.generalAttributeMatrixVectorIndexing = 1;
		resources.limits.generalVaryingIndexing = 1;
		resources.limits.generalSamplerIndexing = 1;
		resources.limits.generalVariableIndexing = 1;
		resources.limits.generalConstantMatrixVectorIndexing = 1;
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

		// Initialize CRC32 table
		InitCRC32();

		// Initialize core graphics objects
		STATUS_CODE coreObjStatus = CoreObjectManager::Get().CreateCoreObjects(window);
		if (coreObjStatus != STATUS_CODE::SUCCESS)
		{
			return coreObjStatus;
		}

		LogWarning("TODO - Waiting for transfer queue to be idle when copying data to buffer");
		LogWarning("TODO - Command buffers are allocated/deallocated every frame");

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE Update(float deltaTime)
	{
		UNUSED(deltaTime);

		DeferredCaller::Get().Update();
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE Shutdown()
	{
		DeferredCaller::Get().Flush();
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

		glslang::TShader shader(GLSLANG_UTILS::ConvertShaderStage(srcData.stage));

		shader.setStrings(&srcData.data, 1);
		shader.setEnvClient(GLSLANG_UTILS::GetClient(), GLSLANG_UTILS::GetClientVersion());

		// Hard-coding at least SPIR-V version 1.4 for now to support ray-tracing when available
		shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_4);

		shader.setEntryPoint(srcData.entryPoint);

		TBuiltInResource resources;
		GetDefaultShaderResources(resources);
		const int defaultVersion = 450; // This is overwritten by #version in the shader src
		const bool forwardCompatible = false;
		const EShMessages messageFlags = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
		EProfile defaultProfile = ENoProfile; // NOTE: Only for desktop, before profiles showed up!

		// Build the includer. If include paths are provided, use the custom filesystem includer.
		// Otherwise, fall back to ForbidIncluder
		std::vector<std::string> includeSearchPaths;
		for (u32 i = 0; i < srcData.includePathCount; i++)
		{
			includeSearchPaths.push_back(srcData.includePaths[i]);
		}

		GlslangIncluder customIncluder{includeSearchPaths};
		glslang::TShader::ForbidIncluder forbidIncluder;
		glslang::TShader::Includer& includer = (srcData.includePathCount > 0)
			? static_cast<glslang::TShader::Includer&>(customIncluder)
			: static_cast<glslang::TShader::Includer&>(forbidIncluder);

		std::string preprocessedStr;
		if (!shader.preprocess(&resources, defaultVersion, defaultProfile, false, forwardCompatible, messageFlags, &preprocessedStr, includer))
		{
			LogError("Failed to preprocess %s shader! Got error: \"%s\"", shaderStageStr, shader.getInfoLog());
			return STATUS_CODE::ERR_INTERNAL;
		}

		const char* preprocessedSources[1] = { preprocessedStr.c_str() };
		shader.setStrings(preprocessedSources, 1);

		if (!shader.parse(&resources, defaultVersion, defaultProfile, false, forwardCompatible, messageFlags, includer))
		{
			LogError("Failed to parse %s shader! Got error: \"%s\"", shaderStageStr, shader.getInfoLog());
			return STATUS_CODE::ERR_INTERNAL;
		}

		glslang::TProgram program;
		program.addShader(&shader);
		if (!program.link(messageFlags))
		{
			LogError("Failed to link %s shader! Got error: \"%s\"", shaderStageStr, program.getInfoLog());
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Convert the intermediate generated by glslang to Spir-V
		glslang::TIntermediate& intermediateRef = *(program.getIntermediate(GLSLANG_UTILS::ConvertShaderStage(srcData.stage)));
		std::vector<uint32_t> spirv;
		glslang::SpvOptions options{};
		options.validate = true;
		options.disableOptimizer = (srcData.optimizationLevel == SHADER_OPTIMIZATION_LEVEL::NONE);
		options.optimizeSize = (srcData.optimizationLevel == SHADER_OPTIMIZATION_LEVEL::SIZE);
		glslang::GlslangToSpv(intermediateRef, spirv, &options); // NOTE - It's also possible to pass in a logger to this function. Maybe we'll want to do that in the future...

		u32 size = static_cast<u32>(spirv.size());
		out_result.data = std::shared_ptr<u32[]>(new u32[size]);
		out_result.size = size;

		// Copy the memory into our own struct
		memcpy(out_result.data.get(), spirv.data(), size * sizeof(u32));

		// Optionally perform reflection
		if (srcData.performReflection)
		{
			const u32 reflectionOptions = EShReflectionDefault | EShReflectionAllIOVariables;
			if (!program.buildReflection(reflectionOptions))
			{
				LogError("Failed to perform shader reflection for %s shader! Got error: \"%s\"", shaderStageStr, shader.getInfoLog());
				return STATUS_CODE::ERR_INTERNAL;
			}

			// UNIFORMS
			{
				u32 uniformCount = static_cast<u32>(program.getNumUniformVariables());
				out_result.reflectionData.uniforms = std::shared_ptr<ShaderUniformData[]>(new ShaderUniformData[uniformCount]);
				out_result.reflectionData.uniformCount = uniformCount;

				for (u32 i = 0; i < uniformCount; i++)
				{
					const glslang::TObjectReflection& reflectedObject = program.getUniform(i);

					ShaderUniformData& uniformData = out_result.reflectionData.uniforms[i];
					uniformData.name = reflectedObject.name.c_str();
					uniformData.binding = reflectedObject.index;
					uniformData.size = reflectedObject.size;
					uniformData.stages = GLSLANG_UTILS::ConvertShaderStageFlags(reflectedObject.stages);
					uniformData.offset = reflectedObject.offset;
				}
			}

			// LOCAL SIZE
			{
				if (srcData.stage == SHADER_STAGE::COMPUTE)
				{
					out_result.reflectionData.localSize.SetX(program.getLocalSize(0));
					out_result.reflectionData.localSize.SetY(program.getLocalSize(1));
					out_result.reflectionData.localSize.SetZ(program.getLocalSize(2));
				}
			}

			// INPUTS
			{
				u32 inputCount = static_cast<u32>(program.getNumPipeInputs());
				if (inputCount > 0)
				{
					out_result.reflectionData.inputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[inputCount]);
					out_result.reflectionData.inputCount = inputCount;

					for (u32 i = 0; i < inputCount; i++)
					{
						const glslang::TObjectReflection& reflectedObject = program.getPipeInput(i);

						const u32 vectorSize = reflectedObject.getType()->getVectorSize();
						const glslang::TBasicType basicType = reflectedObject.getType()->getBasicType();

						ShaderIOData& inputData = out_result.reflectionData.inputs[i];
						inputData.name = reflectedObject.name.c_str();
						inputData.format = GLSLANG_UTILS::ConvertIOTypeToBaseFormat(basicType, vectorSize);
						inputData.location = program.getReflectionPipeIOIndex(reflectedObject.name.c_str(), true);
						inputData.binding = 0; // TODO
					}
				}
			}

			//OUTPUTS
			{
				u32 outputCount = static_cast<u32>(program.getNumPipeOutputs());
				if (outputCount > 0)
				{
					out_result.reflectionData.outputs = std::shared_ptr<ShaderIOData[]>(new ShaderIOData[outputCount]);
					out_result.reflectionData.outputCount = outputCount;

					for (u32 i = 0; i < outputCount; i++)
					{
						const glslang::TObjectReflection& reflectedObject = program.getPipeOutput(i);

						const u32 vectorSize = reflectedObject.getType()->getVectorSize();
						const glslang::TBasicType basicType = reflectedObject.getType()->getBasicType();

						ShaderIOData& outputData = out_result.reflectionData.outputs[i];
						outputData.name = reflectedObject.name.c_str();
						outputData.format = GLSLANG_UTILS::ConvertIOTypeToBaseFormat(basicType, vectorSize);
						outputData.location = program.getReflectionPipeIOIndex(reflectedObject.name.c_str(), false);
						outputData.binding = 0; // TODO
					}
				}
			}

			out_result.reflectionData.isValid = true;
		}

		return STATUS_CODE::SUCCESS;
	}
}