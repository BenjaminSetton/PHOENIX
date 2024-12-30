
#include "shader_vk.h"

#include "../../utils/logger.h"
#include "render_device_vk.h"

namespace PHX
{
	ShaderVk::ShaderVk(RenderDeviceVk* pRenderDevice, const ShaderCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Attempting to create shader of type %u with null render device!", static_cast<u32>(createInfo.type));
			return;
		}

		VkShaderModuleCreateInfo createInfoVk{};
		createInfoVk.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfoVk.codeSize = createInfo.byteCount;
		createInfoVk.pCode = reinterpret_cast<u32*>(createInfo.pBytecode);

		if (vkCreateShaderModule(pRenderDevice->GetLogicalDevice(), &createInfoVk, nullptr, &m_shader) != VK_SUCCESS)
		{
			LogError("Failed to create shader of type %u", static_cast<u32>(createInfo.type));
			return;
		}

		m_type = createInfo.type;
	}

	SHADER_TYPE ShaderVk::GetType() const
	{
		return m_type;
	}
}