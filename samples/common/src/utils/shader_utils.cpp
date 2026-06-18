
#include <fstream>
#include <sstream>
#include <string>

#include "shader_utils.h"

namespace Common
{
	bool AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle renderDevice, PHX::ShaderHandle& shader)
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

		PHX::ShaderSourceData shaderSrc;
		shaderSrc.data = shaderStr.c_str();
		shaderSrc.entryPoint = "main";
		shaderSrc.stage = stage;
		shaderSrc.origin = PHX::SHADER_ORIGIN::GLSL;

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