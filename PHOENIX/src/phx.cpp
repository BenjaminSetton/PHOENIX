
#include <SPIRV/GlslangToSpv.h>

#include "PHX/phx.h"

#include "utils/logger.h"
#include "utils/object_factory.h"
#include "utils/sanity.h"
#include "utils/shader_type_converter.h"

static struct GlobalGraphicsData
{
	bool m_isInitialized = false;
	PHX::GRAPHICS_API m_selectedAPI;
	std::shared_ptr<PHX::IWindow> m_pWindow;
	std::shared_ptr<PHX::IRenderDevice> m_pRenderDevice;
	std::shared_ptr<PHX::ISwapChain> m_pSwapChain;
} g_Data;

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

namespace PHX
{
	STATUS_CODE CreateWindow(const WindowCreateInfo& createInfo)
	{
		g_Data.m_pWindow = OBJ_FACTORY::CreateWindow(createInfo);

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE InitializeGraphics(GRAPHICS_API api)
	{
		if (g_Data.m_pWindow.get() == nullptr)
		{
			LogError("Failed to initialize graphics. Window hasn't been initialized yet!");
			return STATUS_CODE::ERR;
		}

		STATUS_CODE coreObjStatus = OBJ_FACTORY::CreateCoreObjects(api, g_Data.m_pWindow);
		if (coreObjStatus != STATUS_CODE::SUCCESS)
		{
			return coreObjStatus;
		}

		g_Data.m_isInitialized = true;
		g_Data.m_selectedAPI = api;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CreateRenderDevice(const RenderDeviceCreateInfo& createInfo)
	{
		if (!g_Data.m_isInitialized)
		{
			LogError("Failed to create render device. Graphics has not been initialized through InitializeGraphics()!");
			return STATUS_CODE::ERR;
		}

		auto pRenderDevice = OBJ_FACTORY::CreateRenderDevice(createInfo, g_Data.m_selectedAPI);
		g_Data.m_pRenderDevice = pRenderDevice;

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE CreateSwapChain(const SwapChainCreateInfo& createInfo)
	{
		if (!g_Data.m_isInitialized)
		{
			LogError("Failed to create swap chain. Graphics has not been initialized through InitializeGraphics()!");
			return STATUS_CODE::ERR;
		}

		auto pSwapChain = OBJ_FACTORY::CreateSwapChain(createInfo, g_Data.m_selectedAPI);
		g_Data.m_pSwapChain = pSwapChain;

		// TODO - Figure out a way to capture errors. Catch exceptions or change OBJ_FACTORY return type?
		return STATUS_CODE::SUCCESS;
	}

	std::shared_ptr<IWindow> GetWindow()
	{
		return g_Data.m_pWindow;
	}

	std::shared_ptr<IRenderDevice> GetRenderDevice()
	{
		return g_Data.m_pRenderDevice;
	}

	std::shared_ptr<ISwapChain> GetSwapChain()
	{
		return g_Data.m_pSwapChain;
	}

	STATUS_CODE CompileShader(const ShaderSourceData& srcData, CompiledShader& out_result)
	{
		if (srcData.data == nullptr)
		{
			return STATUS_CODE::ERR;
		}

		glslang::TShader shader(SHADER_UTILS::ConvertShaderKind(srcData.kind));

		shader.setStrings(&srcData.data, 1);

		glslang::EShTargetClientVersion targetApiVersion = glslang::EShTargetVulkan_1_0;
		shader.setEnvClient(glslang::EShClientVulkan, targetApiVersion);

		glslang::EShTargetLanguageVersion spirvVersion = glslang::EShTargetSpv_1_0;
		shader.setEnvTarget(glslang::EshTargetSpv, spirvVersion);

		shader.setEntryPoint("main"); // Only use "main" for now

		TBuiltInResource resources;
		GetDefaultShaderResources(resources);
		const int defaultVersion = 450; // This is overwritten by #version in the shader src
		const bool forwardCompatible = false;
		const EShMessages messageFlags = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
		EProfile defaultProfile = ENoProfile; // NOTE: Only for desktop, before profiles showed up!

		// Forbid includes in shaders for now...
		glslang::TShader::ForbidIncluder includer;

		std::string preprocessedStr;
		if (!shader.preprocess(&resources, defaultVersion, defaultProfile, false, forwardCompatible, messageFlags, &preprocessedStr, includer))
		{
			LogError("Failed to preprocess shader: %s", shader.getInfoLog());
			return STATUS_CODE::ERR;
		}

		const char* preprocessedSources[1] = { preprocessedStr.c_str() };
		shader.setStrings(preprocessedSources, 1);

		if (!shader.parse(&resources, defaultVersion, defaultProfile, false, forwardCompatible, messageFlags, includer))
		{
			LogError("Failed to parse shader: %s", shader.getInfoLog());
			return STATUS_CODE::ERR;
		}

		glslang::TProgram program;
		program.addShader(&shader);
		if (!program.link(messageFlags))
		{
			LogError("Failed to link shader: %s", program.getInfoLog());
			return STATUS_CODE::ERR;
		}

		// Convert the intermediate generated by glslang to Spir-V
		glslang::TIntermediate& intermediateRef = *(program.getIntermediate(SHADER_UTILS::ConvertShaderKind(srcData.kind)));
		std::vector<uint32_t> spirv;
		glslang::SpvOptions options{};
		options.validate = true;
		glslang::GlslangToSpv(intermediateRef, spirv, &options); // NOTE - It's also possible to pass in a logger to this function. Maybe we'll want to do that in the future...

		u32 size = static_cast<u32>(spirv.size());
		out_result.data = std::shared_ptr<u32[]>(new u32[size]);
		out_result.size = size;

		// Copy the memory into our own struct
		memcpy(out_result.data.get(), spirv.data(), size * sizeof(u32));

		return STATUS_CODE::SUCCESS;
	}
}