#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "core/interface_types/uniform_interface.h"
#include "PHX/interface/uniform.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class UniformCollectionVk : public IUniformCollection
	{
	public:

		explicit UniformCollectionVk(RenderDeviceVk* pRenderDevice, const UniformCollectionCreateInfo& createInfo);
		~UniformCollectionVk();

		u32 GetGroupCount() const override;
		const UniformDataGroup* GetGroup(u32 groupIndex) const override;
		UniformDataGroup* GetGroup(u32 groupIndex) override;

		STATUS_CODE QueueBufferUpdate(BufferHandle buffer, u32 set, u32 binding, u64 offset, u64 size) override;
		STATUS_CODE QueueImageUpdate(TextureHandle texture, u32 set, u32 binding, u32 imageViewIndex, u32 arrayElement) override;
		STATUS_CODE QueueAccelerationStructureUpdate(AccelerationStructureHandle accelerationStructure, u32 set, u32 binding) override;
		STATUS_CODE Flush(u32 frameIndex);

		const VkDescriptorSet* GetDescriptorSets(u32 frameIndex) const;
		u32 GetDescriptorSetCount(u32 frameIndex) const;

		const VkDescriptorSetLayout* GetDescriptorSetLayouts() const;
		u32 GetDescriptorSetLayoutCount() const;

	private:

		void CacheUniformGroupData(const UniformDataGroup* pDataGroups, u32 groupCount);
		bool IsImageInAppropriateLayout(VkImageLayout layout) const;

		UNIFORM_TYPE GetUniformType(u32 set, u32 binding) const;

	private:

		RenderDeviceVk* m_pRenderDevice;

		std::vector<UniformDataGroup> m_uniformGroups;
		std::vector<UniformData> m_uniforms;

		std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

		// Per-frame-in-flight descriptor sets. Outer index = frame index, inner index = set index
		std::vector<std::vector<VkDescriptorSet>> m_perFrameDescriptorSets;

		// Descriptor writes (queued until FlushForFrame patches dstSet and calls vkUpdateDescriptorSets)
		std::vector<VkWriteDescriptorSet> m_descriptorWrites;
		std::vector<u32> m_writeSetIndices; // Parallel to m_descriptorWrites, tracks which set index each write targets
		std::vector<VkDescriptorBufferInfo> m_writeBufferInfo;
		std::vector<VkDescriptorImageInfo> m_writeImageInfo;
		std::vector<VkWriteDescriptorSetAccelerationStructureKHR> m_writeAccelerationStructureInfo;
		std::vector<VkAccelerationStructureKHR> m_writeAccelerationStructureHandles;
	};
}