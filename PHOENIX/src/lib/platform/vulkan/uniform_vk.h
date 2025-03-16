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

		STATUS_CODE QueueBufferUpdate(u32 set, u32 binding, u32 offset, IBuffer* pBuffer) override;
		STATUS_CODE QueueImageUpdate(u32 set, u32 binding, u32 imageViewIndex, ITexture* pTexture) override;
		STATUS_CODE FlushUpdateQueue() override;

		const VkDescriptorSet* GetDescriptorSets() const;
		u32 GetDescriptorSetCount() const;

		const VkDescriptorSetLayout* GetDescriptorSetLayouts() const;
		u32 GetDescriptorSetLayoutCount() const;

	private:

		RenderDeviceVk* m_renderDevice;

		std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
		std::vector<VkDescriptorSet> m_descriptorSets;

		// Descriptor writes
		std::vector<VkWriteDescriptorSet> m_descriptorWrites;
		std::vector<VkDescriptorBufferInfo> m_writeBufferInfo;
		std::vector<VkDescriptorImageInfo> m_writeImageInfo;
	};
}