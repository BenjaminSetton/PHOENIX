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

		SHADER_KIND GetType() const override;

	private:

		VkShaderModule m_shader;

		SHADER_KIND m_type;
	};

}