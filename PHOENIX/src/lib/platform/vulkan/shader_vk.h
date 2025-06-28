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
		~ShaderVk();

		SHADER_STAGE GetStage() const override;
		const ShaderReflectionData& GetReflectionData() const override;

		VkShaderModule GetShaderModule() const;

	private:

		RenderDeviceVk* m_pRenderDevice;

		VkShaderModule m_shader;
		SHADER_STAGE m_stage;

		ShaderReflectionData m_reflectionData;
	};

}