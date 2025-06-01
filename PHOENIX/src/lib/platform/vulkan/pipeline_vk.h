#pragma once

#include <vulkan/vulkan.h>

#include "PHX/interface/pipeline.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class PipelineVk : public IPipeline
	{
	public:

		PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo);
		PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo);
		~PipelineVk();

		VkPipeline GetPipeline() const;
		VkPipelineLayout GetLayout() const;
		VkPipelineBindPoint GetBindPoint() const;

	private:

		STATUS_CODE CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo);
		STATUS_CODE CreateComputePipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo);

		STATUS_CODE VerifyCreateInfo(const GraphicsPipelineDesc& createInfo);
		STATUS_CODE VerifyCreateInfo(const ComputePipelineDesc& createInfo);

		VkPipelineLayout CreatePipelineLayout(VkDevice logicalDevice, IUniformCollection* pUniformCollection);

	private:

		RenderDeviceVk* m_pRenderDevice;

		VkPipeline m_pipeline;
		VkPipelineLayout m_layout;
		VkPipelineBindPoint m_bindPoint;
	};
}