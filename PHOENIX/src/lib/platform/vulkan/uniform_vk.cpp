
#include <vulkan/vk_enum_string_helper.h>

#include "uniform_vk.h"

#include "../../utils/logger.h"
#include "../../utils/sanity.h"
#include "buffer_vk.h"
#include "render_device_vk.h"
#include "texture_vk.h"
#include "utils/shader_type_converter.h"
#include "utils/uniform_type_converter.h"

namespace PHX
{
	static constexpr u32 MAX_DESCRIPTOR_WRITE_ARRAY_SIZE = 50;

	UniformCollectionVk::UniformCollectionVk(RenderDeviceVk* pRenderDevice, const UniformCollectionCreateInfo& createInfo)
	{
		LogWarning("TODO - Set parameter is not guaranteed to match order in internal vkDescriptorSet array!");

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

		VkResult res = VK_SUCCESS;

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();
		m_descriptorSets.resize(createInfo.groupCount);
		m_descriptorSetLayouts.resize(createInfo.groupCount);
		m_uniformGroups.reserve(createInfo.groupCount);

		for (u32 i = 0; i < createInfo.groupCount; i++)
		{
			const UniformDataGroup& dataGroup = createInfo.dataGroups[i];

			// Create descriptor bindings for each descriptor set
			const u32 numBindings = dataGroup.uniformArrayCount;
			m_uniforms.reserve(numBindings);

			std::vector<VkDescriptorSetLayoutBinding> setBindings(numBindings);
			for (u32 j = 0; j < numBindings; j++)
			{
				const UniformData& uniformData = dataGroup.uniformArray[j];

				VkDescriptorSetLayoutBinding& vkSetLayoutBinding = setBindings[i];
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

			VkDescriptorSetLayout& vkDescriptorSetLayout = m_descriptorSetLayouts.at(i);
			res = vkCreateDescriptorSetLayout(logicalDevice, &vkCreateInfo, nullptr, &vkDescriptorSetLayout);
			if (res != VK_SUCCESS)
			{
				LogError("Failed to create descriptor set layout #%u! Got error: %s", i, string_VkResult(res));
				continue;
			}
		}

		// Create descriptor sets
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pRenderDevice->GetDescriptorPool();
		allocInfo.descriptorSetCount = static_cast<u32>(m_descriptorSetLayouts.size());
		allocInfo.pSetLayouts = m_descriptorSetLayouts.data();

		res = vkAllocateDescriptorSets(logicalDevice, &allocInfo, m_descriptorSets.data());
		if (res != VK_SUCCESS)
		{
			LogError("Failed to allocate descriptor sets! Got error: %s", string_VkResult(res));
			return;
		}

		m_renderDevice = pRenderDevice;

		// Allocate enough space in the write descriptor arrays for a maximum amount of write entries
		m_writeBufferInfo.reserve(MAX_DESCRIPTOR_WRITE_ARRAY_SIZE);
		m_writeImageInfo.reserve(MAX_DESCRIPTOR_WRITE_ARRAY_SIZE);
		m_descriptorWrites.reserve(2 * MAX_DESCRIPTOR_WRITE_ARRAY_SIZE); // Must hold MAX image _and_ buffer write requests

		// Copy the uniform data to internal cache
		CacheUniformGroupData(createInfo.dataGroups, createInfo.groupCount);
	}

	UniformCollectionVk::~UniformCollectionVk()
	{
		// VkDescriptorSet is cleaned up by deleting the descriptor pool that allocated it
		// This is done by the render device itself, since it owns the pool

		for (auto& setLayout : m_descriptorSetLayouts)
		{
			vkDestroyDescriptorSetLayout(m_renderDevice->GetLogicalDevice(), setLayout, nullptr);
		}
		m_descriptorSetLayouts.clear();
	}

	u32 UniformCollectionVk::GetGroupCount() const
	{
		return static_cast<u32>(m_uniformGroups.size());
	}

	const UniformDataGroup* UniformCollectionVk::GetGroup(u32 groupIndex) const
	{
		if (groupIndex >= m_uniformGroups.size())
		{
			return nullptr;
		}

		return &(m_uniformGroups.at(groupIndex));
	}

	UniformDataGroup* UniformCollectionVk::GetGroup(u32 groupIndex)
	{
		if (groupIndex >= m_uniformGroups.size())
		{
			return nullptr;
		}

		return &(m_uniformGroups.at(groupIndex));
	}

	STATUS_CODE UniformCollectionVk::QueueBufferUpdate(u32 set, u32 binding, u32 offset, IBuffer* pBuffer)
	{
		BufferVk* bufferVk = static_cast<BufferVk*>(pBuffer);
		if (bufferVk == nullptr)
		{
			LogError("Failed to queue buffer update! Buffer is null");
			return STATUS_CODE::ERR_API;
		}

		if (set >= m_descriptorSets.size())
		{
			LogError("Failed to queue buffer update! Set number is invalid (expected 0 to %u)", static_cast<u32>(m_descriptorSets.size()));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkDescriptorSet vkDescSet = m_descriptorSets.at(set);

		m_writeBufferInfo.push_back({});
		VkDescriptorBufferInfo& bufferInfo = m_writeBufferInfo.back();
		bufferInfo.buffer = bufferVk->GetBuffer();
		bufferInfo.offset = offset;
		bufferInfo.range = bufferVk->GetSize();

		VkWriteDescriptorSet writeDescSet{};
		writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescSet.dstSet = vkDescSet;
		writeDescSet.dstBinding = binding;
		writeDescSet.dstArrayElement = 0;
		writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescSet.descriptorCount = 1;
		writeDescSet.pBufferInfo = &bufferInfo;

		m_descriptorWrites.push_back(writeDescSet);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE UniformCollectionVk::QueueImageUpdate(u32 set, u32 binding, u32 imageViewIndex, ITexture* pTexture)
	{
		TextureVk* textureVk = static_cast<TextureVk*>(pTexture);
		if (textureVk == nullptr)
		{
			LogError("Failed to queue image update! Texture is null");
			return STATUS_CODE::ERR_API;
		}

		VkImageView imageView = textureVk->GetImageViewAt(imageViewIndex);
		if (imageView == VK_NULL_HANDLE)
		{
			LogError("Failed to queue image update! Image view at index %u is null", imageViewIndex);
			return STATUS_CODE::ERR_API;
		}

		if (set >= m_descriptorSets.size())
		{
			LogError("Failed to queue image update! Set number is invalid (expected 0 to %u)", static_cast<u32>(m_descriptorSets.size()));
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkImageLayout texLayout = textureVk->GetLayout();
		if (texLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && texLayout != VK_IMAGE_LAYOUT_GENERAL)
		{
			LogWarning("Attempting to update image descriptor on binding %u with texture resource that does not have a layout appropriate for shader read/write operations!", binding);
			return STATUS_CODE::SUCCESS;
		}

		VkDescriptorSet vkDescSet = m_descriptorSets.at(set);


		m_writeImageInfo.push_back({});
		VkDescriptorImageInfo& imageInfo = m_writeImageInfo.back();
		imageInfo.imageLayout = texLayout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = VK_NULL_HANDLE; // TODO

		// TODO - Determine if this is wanted behavior
		VkDescriptorType descType = (imageInfo.sampler == VK_NULL_HANDLE) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		VkWriteDescriptorSet writeDescSet{};
		writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescSet.dstSet = vkDescSet;
		writeDescSet.dstBinding = binding;
		writeDescSet.dstArrayElement = 0;
		writeDescSet.descriptorType = descType;
		writeDescSet.descriptorCount = 1;
		writeDescSet.pImageInfo = &imageInfo;

		m_descriptorWrites.push_back(writeDescSet);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE UniformCollectionVk::FlushUpdateQueue()
	{
		if (m_renderDevice == nullptr)
		{
			LogError("Failed to flush update queue! Render device is null");
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (m_descriptorWrites.size() == 0)
		{
			LogWarning("Attempted to flush update queue while queue was empty!");
			return STATUS_CODE::SUCCESS;
		}

		vkUpdateDescriptorSets(m_renderDevice->GetLogicalDevice(), static_cast<u32>(m_descriptorWrites.size()), m_descriptorWrites.data(), 0, nullptr);

		m_descriptorWrites.clear();
		m_writeImageInfo.clear();
		m_writeBufferInfo.clear();

		return STATUS_CODE::SUCCESS;
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

	void UniformCollectionVk::CacheUniformGroupData(const UniformDataGroup* pDataGroups, u32 groupCount)
	{
		if (pDataGroups == nullptr || groupCount == 0)
		{
			return;
		}

		// Count up the number of uniform groups and uniform data objects
		const u32 uniformGroupCount = groupCount;
		u32 uniformDataCount = 0;

		for (u32 i = 0; i < groupCount; i++)
		{
			const UniformDataGroup& dataGroup = pDataGroups[i];
			uniformDataCount += dataGroup.uniformArrayCount;
		}

		// Cache the uniform data
		m_uniformGroups.reserve(uniformGroupCount);
		m_uniforms.reserve(uniformDataCount);
		for (u32 i = 0; i < groupCount; i++)
		{
			UniformDataGroup dataGroup = pDataGroups[i];
			dataGroup.uniformArray = (m_uniforms.data() + m_uniforms.size()); // Modify the pointer to the internal cache
			m_uniformGroups.push_back(dataGroup);

			for (u32 j = 0; j < dataGroup.uniformArrayCount; j++)
			{
				const UniformData& uniformData = dataGroup.uniformArray[j];
				m_uniforms.push_back(uniformData);
			}
		}
	}
}