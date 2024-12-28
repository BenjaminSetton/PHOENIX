
#include "texture_vk.h"

#include "../../utils/sanity.h"
#include "../../utils/logger.h"
#include "render_device_vk.h"
#include "utils/texture_type_converter.h"

namespace PHX
{
	TextureVk::TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo) :
		m_baseImage(VK_NULL_HANDLE), m_imageViews(), m_width(0), m_height(0), m_format(TEXTURE_FORMAT::INVALID), m_arrayLayers(0), m_mipLevels(0),
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
	}

	TextureVk::TextureVk(const TextureVk&& other)
	{
		TODO();
	}

	void TextureVk::CopyFrom(ITexture* other)
	{
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

	TEXTURE_FORMAT TextureVk::GetFormat() const
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
		imageInfo.format = TEX_UTILS::ConvertTextureFormat(createInfo.format);
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

		VmaAllocation alloc;
		if (vmaCreateImage(pRenderDevice->GetAllocator(), &imageInfo, &allocCreateInfo, &m_baseImage, &alloc, nullptr) != VK_SUCCESS)
		{
			LogError("Failed to create texture!");
			return STATUS_CODE::ERR;
		}

		// Cache some of the image data
		m_bytesPerTexel = GetBytesPerTexel(createInfo.format);

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
		createInfoVk.format = TEX_UTILS::ConvertTextureFormat(m_format);
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

	u32 TextureVk::CalculateMipLevelsFromSize(u32 width, u32 height) const
	{
		double dWidth = static_cast<double>(width);
		double dHeight = static_cast<double>(height);
		double exactMips = log2(std::min(dWidth, dHeight));
		return static_cast<u32>(exactMips);
	}

	u32 TextureVk::GetBytesPerTexel(TEXTURE_FORMAT texFormat) const
	{
		switch (texFormat)
		{
		case TEXTURE_FORMAT::INVALID:
		{
			return 0;
		}
		case TEXTURE_FORMAT::R8_UNORM:
		case TEXTURE_FORMAT::R8_SNORM:
		case TEXTURE_FORMAT::R8_UINT:
		case TEXTURE_FORMAT::R8_SINT:
		case TEXTURE_FORMAT::S8_UINT:
		{
			return 1;
		}
		case TEXTURE_FORMAT::R8G8_UNORM:
		case TEXTURE_FORMAT::R8G8_SNORM:
		case TEXTURE_FORMAT::R8G8_UINT:
		case TEXTURE_FORMAT::R8G8_SINT:
		case TEXTURE_FORMAT::R16_UNORM:
		case TEXTURE_FORMAT::R16_SNORM:
		case TEXTURE_FORMAT::R16_UINT:
		case TEXTURE_FORMAT::R16_SINT:
		case TEXTURE_FORMAT::R16_FLOAT:
		case TEXTURE_FORMAT::D16_UNORM:
		{
			return 2;
		}
		case TEXTURE_FORMAT::R8G8B8_UNORM:
		case TEXTURE_FORMAT::R8G8B8_SNORM:
		case TEXTURE_FORMAT::R8G8B8_UINT:
		case TEXTURE_FORMAT::R8G8B8_SINT:
		case TEXTURE_FORMAT::D16_UNORM_S8_UINT:
		{
			return 3;
		}
		case TEXTURE_FORMAT::R8G8B8A8_UNORM:
		case TEXTURE_FORMAT::R8G8B8A8_SNORM:
		case TEXTURE_FORMAT::R8G8B8A8_UINT:
		case TEXTURE_FORMAT::R8G8B8A8_SINT:
		case TEXTURE_FORMAT::R16G16_UNORM:
		case TEXTURE_FORMAT::R16G16_SNORM:
		case TEXTURE_FORMAT::R16G16_UINT:
		case TEXTURE_FORMAT::R16G16_SINT:
		case TEXTURE_FORMAT::R16G16_FLOAT:
		case TEXTURE_FORMAT::R32_UINT:
		case TEXTURE_FORMAT::R32_SINT:
		case TEXTURE_FORMAT::R32_FLOAT:
		case TEXTURE_FORMAT::D32_FLOAT:
		case TEXTURE_FORMAT::D24_UNORM_S8_UINT:
		{
			return 4;
		}
		case TEXTURE_FORMAT::D32_FLOAT_S8_UINT:
		{
			return 5;
		}
		case TEXTURE_FORMAT::R16G16B16_UNORM:
		case TEXTURE_FORMAT::R16G16B16_SNORM:
		case TEXTURE_FORMAT::R16G16B16_UINT:
		case TEXTURE_FORMAT::R16G16B16_SINT:
		case TEXTURE_FORMAT::R16G16B16_FLOAT:
		{
			return 6;
		}
		case TEXTURE_FORMAT::R16G16B16A16_UNORM:
		case TEXTURE_FORMAT::R16G16B16A16_SNORM:
		case TEXTURE_FORMAT::R16G16B16A16_UINT:
		case TEXTURE_FORMAT::R16G16B16A16_SINT:
		case TEXTURE_FORMAT::R16G16B16A16_FLOAT:
		case TEXTURE_FORMAT::R32G32_UINT:
		case TEXTURE_FORMAT::R32G32_SINT:
		case TEXTURE_FORMAT::R32G32_FLOAT:
		case TEXTURE_FORMAT::R64_UINT:
		case TEXTURE_FORMAT::R64_SINT:
		case TEXTURE_FORMAT::R64_FLOAT:
		{
			return 8;
		}
		case TEXTURE_FORMAT::R32G32B32_UINT:
		case TEXTURE_FORMAT::R32G32B32_SINT:
		case TEXTURE_FORMAT::R32G32B32_FLOAT:
		{
			return 12;
		}
		case TEXTURE_FORMAT::R32G32B32A32_UINT:
		case TEXTURE_FORMAT::R32G32B32A32_SINT:
		case TEXTURE_FORMAT::R32G32B32A32_FLOAT:
		case TEXTURE_FORMAT::R64G64_UINT:
		case TEXTURE_FORMAT::R64G64_SINT:
		case TEXTURE_FORMAT::R64G64_FLOAT:
		{
			return 16;
		}
		case TEXTURE_FORMAT::R64G64B64_UINT:
		case TEXTURE_FORMAT::R64G64B64_SINT:
		case TEXTURE_FORMAT::R64G64B64_FLOAT:
		{
			return 24;
		}
		case TEXTURE_FORMAT::R64G64B64A64_UINT:
		case TEXTURE_FORMAT::R64G64B64A64_SINT:
		case TEXTURE_FORMAT::R64G64B64A64_FLOAT:
		{
			return 32;
		}
		}

		LogError("Failed to calculate bytes per texel for format %u", static_cast<u32>(texFormat));
		return 0;
	}
}