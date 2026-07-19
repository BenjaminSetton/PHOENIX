
#include <fstream>
#include <sstream>
#include <string>

#include "shader_utils.h"

namespace Common
{
	const std::vector<std::string>& GetCommonShaderIncludePath()
	{
		static const std::vector<std::string> paths = { "../../common/src/shaders" };
		return paths;
	}

	bool AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle renderDevice, PHX::ShaderHandle& shader, const std::vector<std::string>& includePaths)
	{
		PHX::STATUS_CODE result = PHX::STATUS_CODE::SUCCESS;

		std::ifstream shaderFile;
		shaderFile.open(shaderName, std::ios::in);
		if (!shaderFile.is_open())
		{
			return false;
		}
		std::stringstream buffer;
		buffer << shaderFile.rdbuf();
		std::string shaderStr = buffer.str();

		// Build include paths
		std::vector<const char*> includePathPtrs;
		includePathPtrs.reserve(includePaths.size());
		for (const std::string& path : includePaths)
		{
			includePathPtrs.push_back(path.c_str());
		}

		PHX::ShaderSourceData shaderSrc;
		shaderSrc.data = shaderStr.c_str();
		shaderSrc.entryPoint = "main";
		shaderSrc.stage = stage;
		shaderSrc.origin = PHX::SHADER_ORIGIN::GLSL;
		shaderSrc.includePaths = includePathPtrs.data();
		shaderSrc.includePathCount = static_cast<PHX::u32>(includePathPtrs.size());

		PHX::CompiledShader shaderRes;
		result = CompileShader(shaderSrc, shaderRes);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			return false;
		}

		PHX::ShaderCreateInfo shaderCI{};
		shaderCI.pBytecode = shaderRes.data.get();
		shaderCI.size = shaderRes.size;
		shaderCI.stage = stage;
		shaderCI.reflectionData = shaderRes.reflectionData;

		result = renderDevice.AllocateShader(shaderCI, shader);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			return false;
		}

		// Success
		return true;
	}
}