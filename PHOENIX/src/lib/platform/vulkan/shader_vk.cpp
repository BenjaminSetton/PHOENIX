
#include "shader_vk.h"

#include "../../utils/logger.h"
#include "render_device_vk.h"

namespace PHX
{
	ShaderVk::ShaderVk(RenderDeviceVk* pRenderDevice, const ShaderCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create shader of type %u with null render device!", static_cast<u32>(createInfo.stage));
			return;
		}

		if (createInfo.pBytecode == nullptr)
		{
			LogError("Attempting to create a shader of type %u with no bytecode!", static_cast<u32>(createInfo.stage));
			return;
		}

		if (createInfo.size == 0)
		{
			LogError("Attempting to create a shader of type %u with a size of 0!", static_cast<u32>(createInfo.stage));
			return;
		}

		VkShaderModuleCreateInfo createInfoVk{};
		createInfoVk.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfoVk.codeSize = createInfo.size * sizeof(u32);
		createInfoVk.pCode = createInfo.pBytecode;

		if (vkCreateShaderModule(pRenderDevice->GetLogicalDevice(), &createInfoVk, nullptr, &m_shader) != VK_SUCCESS)
		{
			LogError("Failed to create shader of type %u", static_cast<u32>(createInfo.stage));
			return;
		}

		m_stage = createInfo.stage;
	}

	SHADER_STAGE ShaderVk::GetStage() const
	{
		return m_stage;
	}

	VkShaderModule ShaderVk::GetShaderModule() const
	{
		return m_shader;
	}
}