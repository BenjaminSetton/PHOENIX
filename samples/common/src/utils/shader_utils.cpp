
#include <fstream>
#include <sstream>
#include <string>

#include "shader_utils.h"

namespace Common
{
	PHX::IShader* AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::IRenderDevice* pRenderDevice)
	{
		PHX::STATUS_CODE result = PHX::STATUS_CODE::SUCCESS;

		std::ifstream shaderFile;
		shaderFile.open(shaderName, std::ios::in);
		if (!shaderFile.is_open())
		{
			return nullptr;
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
			return nullptr;
		}

		PHX::ShaderCreateInfo shaderCI{};
		shaderCI.pBytecode = shaderRes.data.get();
		shaderCI.size = shaderRes.size;
		shaderCI.stage = stage;
		shaderCI.reflectionData = shaderRes.reflectionData;

		PHX::IShader* pShader = nullptr;
		result = pRenderDevice->AllocateShader(shaderCI, &pShader);
		if (result != PHX::STATUS_CODE::SUCCESS)
		{
			return nullptr;
		}

		return pShader;
	}
}