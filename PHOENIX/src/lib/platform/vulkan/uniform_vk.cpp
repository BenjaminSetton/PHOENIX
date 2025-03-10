
#include "uniform_vk.h"

#include "../../utils/sanity.h"
#include "../../utils/logger.h"
#include "render_device_vk.h"
#include "utils/shader_type_converter.h"
#include "utils/uniform_type_converter.h"

namespace PHX
{
	UniformCollectionVk::UniformCollectionVk(RenderDeviceVk* pRenderDevice, const UniformCollectionCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}

		if (createInfo.groupCount == 0)
		{
			return;
		}

		if (createInfo.dataGroups == nullptr)
		{
			return;
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();
		m_descriptorSets.reserve(createInfo.groupCount);
		m_descriptorSetLayouts.reserve(createInfo.groupCount);

		for (u32 i = 0; i < createInfo.groupCount; i++)
		{
			const UniformDataGroup& dataGroup = createInfo.dataGroups[i];

			// Create descriptor bindings for each descriptor set
			const u32 numBindings = dataGroup.uniformArrayCount;
			std::vector<VkDescriptorSetLayoutBinding> setBindings(numBindings);
			for (u32 j = 0; j < numBindings; j++)
			{
				const UniformData& uniformData = dataGroup.uniformArray[j];

				VkDescriptorSetLayoutBinding vkSetLayoutBinding{};
				vkSetLayoutBinding.binding = uniformData.binding;
				vkSetLayoutBinding.descriptorType = UNIFORM_UTILS::ConvertUniformType(uniformData.type);
				vkSetLayoutBinding.descriptorCount = 1;
				vkSetLayoutBinding.stageFlags = SHADER_UTILS::ConvertShaderStage(uniformData.shaderStage);
				vkSetLayoutBinding.pImmutableSamplers = nullptr; // Optional
			}

			// Create descriptor set layouts
			VkDescriptorSetLayoutCreateInfo vkCreateInfo{};
			vkCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			vkCreateInfo.flags = 0; // TODO - Add descriptor set flags to create info somehow
			vkCreateInfo.pBindings = setBindings.data();
			vkCreateInfo.bindingCount = static_cast<u32>(setBindings.size());

			VkDescriptorSetLayout vkDescriptorSetLayout = m_descriptorSetLayouts.at(i);
			if (vkCreateDescriptorSetLayout(logicalDevice, &vkCreateInfo, nullptr, &vkDescriptorSetLayout) != VK_SUCCESS)
			{
				LogError("Failed to create descriptor set layout #%u!", i);
				continue;
			}
		}

		// Create descriptor sets
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pRenderDevice->GetDescriptorPool();
		allocInfo.descriptorSetCount = static_cast<u32>(m_descriptorSetLayouts.size());
		allocInfo.pSetLayouts = m_descriptorSetLayouts.data();

		if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
		{
			LogError("Failed to allocate descriptor sets!");
			return;
		}
	}

	UniformCollectionVk::~UniformCollectionVk()
	{
		for (auto& setLayout : m_descriptorSets)
		{
			setLayout = VK_NULL_HANDLE;
		}
	}

	u32 UniformCollectionVk::GetGroupCount() const
	{
		TODO();
		return 0;
	}

	const UniformDataGroup& UniformCollectionVk::GetGroup(u32 groupIndex) const
	{
		TODO();

		UNUSED(groupIndex);
		static UniformDataGroup temp;
		return temp;
	}

	UniformDataGroup& UniformCollectionVk::GetGroup(u32 groupIndex)
	{
		TODO();

		UNUSED(groupIndex);
		static UniformDataGroup temp;
		return temp;
	}

	const VkDescriptorSet* UniformCollectionVk::GetDescriptorSets() const
	{
		return m_descriptorSets.data();
	}

	u32 UniformCollectionVk::GetDescriptorSetCount() const
	{
		return static_cast<u32>(m_descriptorSets.size());
	}

	const VkDescriptorSetLayout* UniformCollectionVk::GetDescriptorSetLayouts() const
	{
		return m_descriptorSetLayouts.data();
	}

	u32 UniformCollectionVk::GetDescriptorSetLayoutCount() const
	{
		return static_cast<u32>(m_descriptorSetLayouts.size());
	}
}