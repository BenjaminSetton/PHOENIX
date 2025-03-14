
#include "texture_vk.h"

#include "render_device_vk.h"
#include "utils/logger.h"
#include "utils/sanity.h"
#include "utils/texture_type_converter.h"
#include "utils/texture_utils.h"

namespace PHX
{
	TextureVk::TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo) :
		m_baseImage(VK_NULL_HANDLE), m_imageViews(), m_layout(VK_IMAGE_LAYOUT_UNDEFINED), m_width(0), m_height(0), m_format(BASE_FORMAT::INVALID), m_arrayLayers(0), m_mipLevels(0),
		m_viewType(VIEW_TYPE::INVALID), m_viewScope(VIEW_SCOPE::INVALID), m_minFilter(FILTER_MODE::INVALID), m_magFilter(FILTER_MODE::INVALID),
		m_sampAddressMode(SAMPLER_ADDRESS_MODE::INVALID), m_sampFilter(FILTER_MODE::INVALID), m_anisotropicFilteringEnabled(false), m_anisotropyLevel(0.0f)
	{
		RenderDeviceVk* renderDeviceVk = static_cast<RenderDeviceVk*>(pRenderDevice);
		if (renderDeviceVk == nullptr)
		{
			LogError("Failed to create texture! Render device is null");
			return;
		}

		if (CreateBaseImage(pRenderDevice, baseCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		if (CreateImageViews(pRenderDevice, viewCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		// Sampler
		TODO();

		m_renderDevice = renderDeviceVk;

		m_minFilter = samplerCreateInfo.minificationFilter;
		m_magFilter = samplerCreateInfo.magnificationFilter;
		m_sampAddressMode = samplerCreateInfo.addressModeUVW;
		m_sampFilter = samplerCreateInfo.samplerMipMapFilter;
		m_anisotropicFilteringEnabled = samplerCreateInfo.enableAnisotropicFiltering;
		m_anisotropyLevel = samplerCreateInfo.maxAnisotropy;
	}

	TextureVk::TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView)
	{
		RenderDeviceVk* renderDeviceVk = static_cast<RenderDeviceVk*>(pRenderDevice);
		if (renderDeviceVk == nullptr)
		{
			LogError("Failed to create texture! Render device is null");
			return;
		}

		if (CreateBaseImage(pRenderDevice, baseCreateInfo) != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_imageViews.push_back(imageView);

		m_renderDevice = renderDeviceVk;
	}

	TextureVk::~TextureVk()
	{
		DestroyImage();
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

	bool TextureVk::IsDepthTexture() const
	{
		switch (m_format)
		{
		case BASE_FORMAT::D16_UNORM:
		case BASE_FORMAT::D32_FLOAT:
		case BASE_FORMAT::D16_UNORM_S8_UINT:
		case BASE_FORMAT::D24_UNORM_S8_UINT:
		case BASE_FORMAT::D32_FLOAT_S8_UINT:
		{
			return true;
		}
		default:
		{
			break;
		}
		}

		return false;
	}

	bool TextureVk::HasStencilComponent() const
	{
		switch (m_format)
		{
		case BASE_FORMAT::S8_UINT:
		case BASE_FORMAT::D16_UNORM_S8_UINT:
		case BASE_FORMAT::D24_UNORM_S8_UINT:
		case BASE_FORMAT::D32_FLOAT_S8_UINT:
		{
			return true;
		}
		default:
		{
			break;
		}
		}

		return false;
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

	VkImageLayout TextureVk::GetLayout() const
	{
		return m_layout;
	}

	void TextureVk::SetLayout(VkImageLayout layout)
	{
		m_layout = layout;
	}

	bool TextureVk::FillTransitionLayoutInfo(VkImageLayout destinationLayout, VkPipelineStageFlags& out_sourceStage, VkPipelineStageFlags& out_destinationStage, QUEUE_TYPE& out_queueType, VkImageMemoryBarrier& out_barrier)
	{
		if (m_layout == destinationLayout)
		{
			// Nothing to do
			return false;
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = m_layout;
		barrier.newLayout = destinationLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_baseImage;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = m_mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = m_arrayLayers;
		barrier.srcAccessMask = 0; // These are set below
		barrier.dstAccessMask = 0; // These are set below

		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_NONE;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_NONE;
		QUEUE_TYPE commandQueueType = QUEUE_TYPE::GRAPHICS;

		if (destinationLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent())
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::TRANSFER;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::TRANSFER;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::TRANSFER;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && destinationLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; // Let's block on the vertex shader for now...

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // I guess this depends on whether we're using the texture in the vertex or pixel shader?

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && destinationLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			// We're probably converting the color attachment after doing the LDR conversion

			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			// We're probably converting the color attachment before doing the LDR conversion

			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && destinationLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_UNDEFINED && destinationLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			// Textures transitioning from these layouts usually transitioned immediately using a disposable command
			// Do access masks and stages really matter that much in this case??
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = 0;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = 0;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_GENERAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_GENERAL && destinationLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_GENERAL && destinationLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else if (m_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && destinationLayout == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			commandQueueType = QUEUE_TYPE::GRAPHICS;
		}
		else
		{
			ASSERT_ALWAYS("Unsupported layout transition!");
		}

		out_sourceStage = sourceStage;
		out_destinationStage = destinationStage;
		out_queueType = commandQueueType;
		out_barrier = barrier;

		return true;
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

		m_layout = imageInfo.initialLayout;
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

	void TextureVk::DestroyImage()
	{
		if (m_renderDevice == nullptr)
		{
			return;
		}

		// Allocation
		vmaDestroyImage(m_renderDevice->GetAllocator(), m_baseImage, m_alloc);

		// Image views
		for (auto& imageView : m_imageViews)
		{
			if (imageView != VK_NULL_HANDLE)
			{
				vkDestroyImageView(m_renderDevice->GetLogicalDevice(), imageView, nullptr);
				imageView = VK_NULL_HANDLE;
			}
		}
		m_imageViews.clear();

		// Image
		if (m_baseImage != VK_NULL_HANDLE)
		{
			vkDestroyImage(m_renderDevice->GetLogicalDevice(), m_baseImage, nullptr);
			m_baseImage = VK_NULL_HANDLE;
		}
	}
}