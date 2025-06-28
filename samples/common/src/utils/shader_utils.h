#pragma once

#include <PHX/phx.h>

namespace Common
{
	[[nodiscard]] PHX::IShader* AllocateShader(const std::string& shaderName, PHX::SHADER_STAGE stage, PHX::IRenderDevice* pRenderDevice);
}