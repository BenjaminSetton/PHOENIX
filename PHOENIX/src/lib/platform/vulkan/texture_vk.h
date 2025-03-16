#pragma once

#include <vector>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "PHX/interface/texture.h"
#include "PHX/types/queue_type.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class TextureVk : public ITexture
	{
	public:

		explicit TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, const TextureViewCreateInfo& viewCreateInfo, const TextureSamplerCreateInfo& samplerCreateInfo);
		explicit TextureVk(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& baseCreateInfo, VkImageView imageView); // Create texture from existing image views (e.g. swap chain image views)
		~TextureVk();
		TextureVk(const TextureVk&& other);

		// Copy operations must be explicitly made through CopyFrom()
		TextureVk(const TextureVk& other) = delete;
		TextureVk& operator=(const TextureVk& other) = delete;

		void CopyFrom(ITexture* other) override;

		u32 GetWidth() const override;
		u32 GetHeight() const override;
		BASE_FORMAT GetFormat() const override;
		u32 GetArrayLayers() const override;
		u32 GetMipLevels() const override;
		SAMPLE_COUNT GetSampleCount() const override;

		VIEW_TYPE GetViewType() const override;
		VIEW_SCOPE GetViewScope() const override;

		FILTER_MODE GetMinificationFilter() const override;
		FILTER_MODE GetMagnificationFilter() const override;
		SAMPLER_ADDRESS_MODE GetSamplerAddressMode() const override;
		FILTER_MODE GetSamplerFilter() const override;
		bool IsAnisotropicFilteringEnabled() const override;
		float GetAnisotropyLevel() const override;

		bool IsDepthTexture() const override;
		bool HasStencilComponent() const override;

		u32 GetNumImageViews() const;
		VkImageView GetImageViewAt(u32 index) const;

		VkImageLayout GetLayout() const;
		void SetLayout(VkImageLayout layout); // Used when device context adds transition commands to command buffer
		bool FillTransitionLayoutInfo(VkImageLayout destinationLayout, VkPipelineStageFlags& out_sourceStage, VkPipelineStageFlags& out_destinationStage, QUEUE_TYPE& out_queueType, VkImageMemoryBarrier& out_barrier);

	private:

		STATUS_CODE CreateBaseImage(RenderDeviceVk* pRenderDevice, const TextureBaseCreateInfo& createInfo, bool createVkImageHandle = true);
		STATUS_CODE CreateImageViews(RenderDeviceVk* pRenderDevice, const TextureViewCreateInfo& createInfo);
		void DestroyImage();

	private:

		RenderDeviceVk* m_renderDevice;

		VkImage m_baseImage;
		std::vector<VkImageView> m_imageViews;
		VmaAllocation m_alloc;
		//VkSampler sampler;
		VkImageLayout m_layout;

		u32 m_width;
		u32 m_height;
		BASE_FORMAT m_format;
		u32 m_arrayLayers;
		u32 m_mipLevels;
		SAMPLE_COUNT m_sampleCount;

		VIEW_TYPE m_viewType;
		VIEW_SCOPE m_viewScope;

		FILTER_MODE m_minFilter;
		FILTER_MODE m_magFilter;
		SAMPLER_ADDRESS_MODE m_sampAddressMode;
		FILTER_MODE m_sampFilter;
		bool m_anisotropicFilteringEnabled;
		float m_anisotropyLevel;

		u32 m_bytesPerTexel;
	};
}