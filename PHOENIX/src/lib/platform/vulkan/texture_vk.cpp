
#include "texture_vk.h"

#include "../../utils/sanity.h"
#include "../../utils/logger.h"
#include "render_device_vk.h"
#include "utils/texture_type_converter.h"
#include "utils/texture_utils.h"

namespace PHX
{
	TextureVk::TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo) :
		m_baseImage(VK_NULL_HANDLE), m_imageViews(), m_width(0), m_height(0), m_format(BASE_FORMAT::INVALID), m_arrayLayers(0), m_mipLevels(0),
		m_viewType(VIEW_TYPE::INVALID), m_viewScope(VIEW_SCOPE::INVALID), m_minFilter(FILTER_MODE::INVALID), m_magFilter(FILTER_MODE::INVALID),
		m_sampAddressMode(SAMPLER_ADDRESS_MODE::INVALID), m_sampFilter(FILTER_MODE::INVALID), m_anisotropicFilteringEnabled(false), m_anisotropyLevel(0.0f)
	{
		if (CreateBaseImage(pRenderDevice, baseCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		if (CreateImageViews(pRenderDevice, viewCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		TODO();

		m_minFilter = samplerCreateInfo.minificationFilter;
		m_magFilter = samplerCreateInfo.magnificationFilter;
		m_sampAddressMode = samplerCreateInfo.addressModeUVW;
		m_sampFilter = samplerCreateInfo.samplerMipMapFilter;
		m_anisotropicFilteringEnabled = samplerCreateInfo.enableAnisotropicFiltering;
		m_anisotropyLevel = samplerCreateInfo.maxAnisotropy;
	}

	TextureVk::TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView)
	{
		if (CreateBaseImage(pRenderDevice, baseCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_imageViews.push_back(imageView);
	}

	TextureVk::~TextureVk()
	{
		TODO();
		// Figure out how to get the allocator from the render device in here
		//vmaDestroyImage(, m_baseImage, m_alloc);
	}

	TextureVk::TextureVk(const TextureVk&& other)
	{
		UNUSED(other);
		TODO();
	}

	void TextureVk::CopyFrom(ITexture* other)
	{
		UNUSED(other);
		TODO();
	}

	u32 TextureVk::GetWidth() const
	{
		return m_width;
	}

	u32 TextureVk::GetHeight() const
	{
		return m_height;
	}

	BASE_FORMAT TextureVk::GetFormat() const
	{
		return m_format;
	}

	u32 TextureVk::GetArrayLayers() const
	{
		return m_arrayLayers;
	}

	u32 TextureVk::GetMipLevels() const
	{
		return m_mipLevels;
	}

	SAMPLE_COUNT TextureVk::GetSampleCount() const
	{
		return m_sampleCount;
	}

	VIEW_TYPE TextureVk::GetViewType() const
	{
		return m_viewType;
	}

	VIEW_SCOPE TextureVk::GetViewScope() const
	{
		return m_viewScope;
	}

	FILTER_MODE TextureVk::GetMinificationFilter() const
	{
		return m_minFilter;
	}

	FILTER_MODE TextureVk::GetMagnificationFilter() const
	{
		return m_magFilter;
	}

	SAMPLER_ADDRESS_MODE TextureVk::GetSamplerAddressMode() const
	{
		return m_sampAddressMode;
	}

	FILTER_MODE TextureVk::GetSamplerFilter() const
	{
		return m_sampFilter;
	}

	bool TextureVk::IsAnisotropicFilteringEnabled() const
	{
		return m_anisotropicFilteringEnabled;
	}

	float TextureVk::GetAnisotropyLevel() const
	{
		return m_anisotropyLevel;
	}

	u32 TextureVk::GetNumImageViews() const
	{
		return static_cast<u32>(m_imageViews.size());
	}

	VkImageView TextureVk::GetImageViewAt(u32 index) const
	{
		if (index < m_imageViews.size())
		{
			return m_imageViews.at(index);
		}

		return VK_NULL_HANDLE;
	}

	STATUS_CODE TextureVk::CreateBaseImage(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& createInfo)
	{
		// Re-calculate mip count, if necessary. Vulkan disallows 0 mip levels
		u32 mipsToUse = 1;
		if (createInfo.mipLevels == 0)
		{
			mipsToUse = CalculateMipLevelsFromSize(createInfo.width, createInfo.height);
			LogWarning("Texture resource specified an invalid 0 mip levels for a %ux%u, using %u mip levels instead", createInfo.width, createInfo.height, mipsToUse);
		}
		else
		{
			mipsToUse = createInfo.mipLevels;
		}

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = createInfo.width;
		imageInfo.extent.height = createInfo.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipsToUse;
		imageInfo.arrayLayers = createInfo.arrayLayers;
		imageInfo.format = TEX_UTILS::ConvertBaseFormat(createInfo.format);
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = TEX_UTILS::ConvertUsageFlags(createInfo.usageFlags);
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = TEX_UTILS::ConvertSampleCount(createInfo.sampleFlags);
		imageInfo.flags = 0; // TEMP - No use for these right now

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocCreateInfo.priority = 1.0f;

		if (vmaCreateImage(pRenderDevice->GetAllocator(), &imageInfo, &allocCreateInfo, &m_baseImage, &m_alloc, nullptr) != VK_SUCCESS)
		{
			LogError("Failed to create texture!");
			return STATUS_CODE::ERR;
		}

		// Cache some of the image data
		m_bytesPerTexel = GetBaseFormatSize(createInfo.format);

		// Calculate mip levels
		if (createInfo.generateMips && mipsToUse > 1)
		{
			// Source layout should always be UNDEFINED here
			TODO();

			//TransitionLayout_Immediate(layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			//GenerateMipmaps_Immediate(_baseImageInfo->mipLevels);
		}

		m_width = createInfo.width;
		m_height = createInfo.height;
		m_arrayLayers = createInfo.arrayLayers;
		m_format = createInfo.format;
		m_sampleCount = createInfo.sampleFlags;
		m_mipLevels = mipsToUse;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE TextureVk::CreateImageViews(RenderDeviceVk* pRenderDevice, const TextureViewCreateInfo& createInfo)
	{
		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		VkImageViewCreateInfo createInfoVk{};
		createInfoVk.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfoVk.image = m_baseImage;
		createInfoVk.viewType = TEX_UTILS::ConvertViewType(createInfo.type);
		createInfoVk.format = TEX_UTILS::ConvertBaseFormat(m_format);
		createInfoVk.subresourceRange.aspectMask = TEX_UTILS::ConvertAspectFlags(createInfo.aspect);
		createInfoVk.subresourceRange.baseMipLevel = 0;
		createInfoVk.subresourceRange.levelCount = m_mipLevels;
		createInfoVk.subresourceRange.baseArrayLayer = 0;
		createInfoVk.subresourceRange.layerCount = 1;

		// Consider view type
		if (createInfo.type == VIEW_TYPE::TYPE_CUBE)
		{
			createInfoVk.subresourceRange.layerCount = 6;
		}

		// Consider view scope
		switch (createInfo.scope)
		{
		case VIEW_SCOPE::ENTIRE:
		{
			m_imageViews.resize(1);
			VkImageView& imageView = m_imageViews.at(0);
			if (vkCreateImageView(logicalDevice, &createInfoVk, nullptr, &imageView) != VK_SUCCESS)
			{
				LogError("Failed to create texture image view!");
				return STATUS_CODE::ERR;
			}

			break;
		}
		case VIEW_SCOPE::PER_MIP:
		{
			createInfoVk.subresourceRange.levelCount = 1;

			m_imageViews.resize(m_mipLevels);
			for (uint32_t i = 0; i < m_mipLevels; i++)
			{
				// We set the base mip level to the current mip level in the iteration and levelCount will remain as 1
				createInfoVk.subresourceRange.baseMipLevel = i;

				VkImageView& imageView = m_imageViews.at(i);
				if (vkCreateImageView(logicalDevice, &createInfoVk, nullptr, &imageView) != VK_SUCCESS)
				{
					LogError("Failed to create texture image view!");
					return STATUS_CODE::ERR;
				}
			}

			break;
		}
		}

		m_viewType = createInfo.type;
		m_viewScope = createInfo.scope;

		return STATUS_CODE::SUCCESS;
	}
}