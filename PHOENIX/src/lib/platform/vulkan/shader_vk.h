#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/shader.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class ShaderVk : public IShader
	{
	public:

		explicit ShaderVk(RenderDeviceVk* pRenderDevice, const ShaderCreateInfo& createInfo);

		SHADER_STAGE GetStage() const override;

		VkShaderModule GetShaderModule() const;

	private:

		VkShaderModule m_shader;

		SHADER_STAGE m_stage;
	};

}