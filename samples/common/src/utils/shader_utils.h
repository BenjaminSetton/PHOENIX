#pragma once

#include <PHX/phx.h>
#include <PHX/interface/shader.h>

namespace Common
{
	bool AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle renderDevice, PHX::ShaderHandle& shader);
}