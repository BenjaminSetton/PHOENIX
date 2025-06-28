
#include <SPIRV/GlslangToSpv.h>

#include "PHX/phx.h"

#include "core/global_settings.h"
#include "core/object_factory.h"
#include "utils/crc32.h"
#include "utils/glslang_type_converter.h"
#include "utils/logger.h"
#include "utils/sanity.h"

static constexpr PHX::u32 VER_MAJOR_SIZE = 8; // 256
static constexpr PHX::u32 VER_MINOR_SIZE = 8; // 256
static constexpr PHX::u32 VER_PATCH_SIZE = 16; // 65536

// PHEONIX VERSION 0.0.1
static constexpr PHX::u32 VER_MAJOR = 0;
static constexpr PHX::u32 VER_MINOR = 0;
static constexpr PHX::u32 VER_PATCH = 1;

#define BUILD_VERSION(major, minor, patch) \
	(major << (VER_MINOR_SIZE + VER_PATCH_SIZE)) | \
	(minor << (VER_PATCH_SIZE)				   ) | \
	(patch << 0								   )

static constexpr const char* SHADER_STAGE_NAMES[static_cast<PHX::u32>(PHX::SHADER_STAGE::MAX)] =
{
	"VERTEX",
	"GEOMETRY",
	"FRAGMENT",
	"COMPUTE",
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

	if (settings.swapChainOutdatedCallback == nullptr)
	{
		PHX::LogError("Mandatory setting \"swapChainOutdatedCallback\" has not been set!");
		isValid = false;
	}

	if (settings.windowFocusChangedCallback == nullptr)
	{
		PHX::LogError("Mandatory setting \"windowFocusChangedCallback\" has not been set!");
		isValid = false;
	}

	if (settings.windowMaximizedCallback == nullptr)
	{
		PHX::LogError("Mandatory setting \"windowMaximizedCallback\" has not been set!");
		isValid = false;
	}

	if (settings.windowMinimizedCallback == nullptr)
	{
		PHX::LogError("Mandatory setting \"windowMinimizedCallback\" has not been set!");
		isValid = false;
	}

	if (settings.windowResizedCallback == nullptr)
	{
		PHX::LogError("Mandatory setting \"windowResizedCallback\" has not been set!");
		isValid = false;
	}

	return isValid;
}

namespace PHX
{
	STATUS_CODE Initialize(const Settings& initSettings, IWindow* pWindow)
	{
		if (pWindow == nullptr)
		{
			LogError("Failed to initialize library! Window is null");
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
		STATUS_CODE coreObjStatus = OBJ_FACTORY::CreateCoreObjects(pWindow);
		if (coreObjStatus != STATUS_CODE::SUCCESS)
		{
			return coreObjStatus;
		}

		LogWarning("TODO - Waiting for transfer queue to be idle when copying data to buffer");
		LogWarning("TODO - Command buffers are allocated/deallocated every frame");

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

	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo, IWindow** out_window)
	{
		if (out_window == nullptr)
		{
			LogError("Failed to create window! out_window is null");
			return STATUS_CODE::ERR_API;
		}

		*out_window = OBJ_FACTORY::CreateWindow(createInfo);

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo, IRenderDevice** out_renderDevice)
	{
		if (out_renderDevice == nullptr)
		{
			LogError("Failed to create render device! out_renderDevice is null");
			return STATUS_CODE::ERR_API;
		}

		*out_renderDevice = OBJ_FACTORY::CreateRenderDevice(createInfo);

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	void DestroyWindow(IWindow** pWindow)
	{
		SAFE_DEL(*pWindow);
	}

	void DestroyRenderDevice(IRenderDevice** pRenderDevice)
	{
		SAFE_DEL(*pRenderDevice);
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

		glslang::EShTargetClientVersion targetApiVersion = glslang::EShTargetVulkan_1_0;
		shader.setEnvClient(glslang::EShClientVulkan, targetApiVersion);

		glslang::EShTargetLanguageVersion spirvVersion = glslang::EShTargetSpv_1_0;
		shader.setEnvTarget(glslang::EshTargetSpv, spirvVersion);

		shader.setEntryPoint(srcData.entryPoint);

		TBuiltInResource resources;
		GetDefaultShaderResources(resources);
		const int defaultVersion = 450; // This is overwritten by #version in the shader src
		const bool forwardCompatible = false;
		const EShMessages messageFlags = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
		EProfile defaultProfile = ENoProfile; // NOTE: Only for desktop, before profiles showed up!

		// Forbid includes in shaders for now...
		LogWarning("Shader includes are not currently supported!");
		glslang::TShader::ForbidIncluder includer;

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
			if (!program.buildReflection())
			{
				LogError("Failed to perform shader reflection for %s shader! Got error: \"\"", shaderStageStr, shader.getInfoLog());
				return STATUS_CODE::ERR_INTERNAL;
			}

			u32 uniformCount = static_cast<u32>(program.getNumUniformVariables());
			out_result.reflectionData.pUniformData = std::shared_ptr<ShaderUniformData[]>(new ShaderUniformData[uniformCount]);

			for (u32 i = 0; i < uniformCount; i++)
			{
				const glslang::TObjectReflection& reflectedObject = program.getUniform(i);

				ShaderUniformData& uniformData = out_result.reflectionData.pUniformData[i];
				uniformData.name = reflectedObject.name.c_str();
				uniformData.index = reflectedObject.index;
				uniformData.size = reflectedObject.size;
				uniformData.stages = GLSLANG_UTILS::ConvertShaderStageFlags(reflectedObject.stages);
			}

			if (srcData.stage == SHADER_STAGE::COMPUTE)
			{
				out_result.reflectionData.localSize.SetX(program.getLocalSize(0));
				out_result.reflectionData.localSize.SetY(program.getLocalSize(1));
				out_result.reflectionData.localSize.SetZ(program.getLocalSize(2));
			}

			out_result.reflectionData.isValid = true;
		}

		return STATUS_CODE::SUCCESS;
	}
}