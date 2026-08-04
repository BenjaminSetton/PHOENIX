#pragma once

#include <string>
#include <vector>
#include <PHX/phx.h>
#include <PHX/interface/shader.h>

namespace Common
{
	// Returns the default include search paths for shared shader include files.
	// Currently returns the path to samples/common/src/shaders/.
	const std::vector<std::string>& GetCommonShaderIncludePath();

	// Determines the PHX::SHADER_ORIGIN to use based on a shader file's extension
	// (e.g. ".slang" -> SLANG, ".hlsl" -> HLSL, anything else -> GLSL).
	PHX::SHADER_ORIGIN GetOriginFromFilePath(const std::string& filePath);

	bool AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle renderDevice, PHX::ShaderHandle& shader, const std::vector<std::string>& includePaths = GetCommonShaderIncludePath());
}