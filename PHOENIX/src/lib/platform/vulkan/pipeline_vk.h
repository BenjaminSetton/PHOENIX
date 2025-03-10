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

		PipelineVk(RenderDeviceVk* pRenderDevice, const GraphicsPipelineCreateInfo& createInfo);
		PipelineVk(RenderDeviceVk* pRenderDevice, const ComputePipelineCreateInfo& createInfo);
		~PipelineVk();

		VkPipeline GetPipeline() const;
		VkPipelineLayout GetLayout() const;
		VkPipelineBindPoint GetBindPoint() const;

	private:

		STATUS_CODE CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, const GraphicsPipelineCreateInfo& createInfo);
		STATUS_CODE CreateComputePipeline(RenderDeviceVk* pRenderDevice, const ComputePipelineCreateInfo& createInfo);

		STATUS_CODE VerifyCreateInfo(const GraphicsPipelineCreateInfo& createInfo);
		STATUS_CODE VerifyCreateInfo(const ComputePipelineCreateInfo& createInfo);

		VkPipelineLayout CreatePipelineLayout(VkDevice logicalDevice, IUniformCollection* pUniformCollection);

	private:

		VkPipeline m_pipeline;
		VkPipelineLayout m_layout;
		VkPipelineBindPoint m_bindPoint;
	};
}