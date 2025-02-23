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

		PipelineVk(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo);
		~PipelineVk();

	private:

		STATUS_CODE CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo);
		STATUS_CODE CreateComputePipeline(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo);
		STATUS_CODE VerifyCreateInfo(const PipelineCreateInfo& createInfo);

	private:

		VkPipeline m_pipeline;
		VkPipelineLayout m_layout;
	};
}