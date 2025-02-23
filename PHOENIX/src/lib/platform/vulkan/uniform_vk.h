#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/interface/uniform.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class UniformCollectionVk : public IUniformCollection
	{
	public:

		UniformCollectionVk(RenderDeviceVk* pRenderDevice, const UniformCollectionCreateInfo& createInfo);
		~UniformCollectionVk();

		u32 GetGroupCount() const override;
		const UniformDataGroup& GetGroup(u32 groupIndex) const override;
		UniformDataGroup& GetGroup(u32 groupIndex) override;

		u32 GetDescriptorSetCount() const;
		VkDescriptorSet GetDescriptorSet(u32 setIndex) const;

		const VkDescriptorSetLayout* GetDescriptorSetLayouts() const;
		u32 GetDescriptorSetLayoutCount() const;

	private:

		std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
		std::vector<VkDescriptorSet> m_descriptorSets;
	};
}