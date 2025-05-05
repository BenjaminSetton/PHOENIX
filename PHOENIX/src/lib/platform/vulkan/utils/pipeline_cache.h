#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "../pipeline_vk.h"

namespace PHX
{
	struct GraphicsPipelineDescHasher
	{
		size_t operator()(const GraphicsPipelineCreateInfo& desc) const;
	};

	struct ComputePipelineDescHasher
	{
		size_t operator()(const ComputePipelineCreateInfo& desc) const;
	};

	class PipelineCache
	{
	public:

		PipelineCache() = default;
		~PipelineCache() = default;

		PipelineCache(const PipelineCache& other) = delete;
		PipelineCache& operator=(const PipelineCache& other) = delete;

		// Graphics pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const GraphicsPipelineCreateInfo& desc);
		PipelineVk* Find(const GraphicsPipelineCreateInfo& desc);
		void Delete(const GraphicsPipelineCreateInfo& desc);

		// Compute pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const ComputePipelineCreateInfo& desc);
		PipelineVk* Find(const ComputePipelineCreateInfo& desc);
		void Delete(const ComputePipelineCreateInfo& desc);

	private:

		// PipelineVk caches
		std::unordered_map<GraphicsPipelineCreateInfo, PipelineVk*, GraphicsPipelineDescHasher> m_graphicsPipelineCache;
		std::unordered_map<ComputePipelineCreateInfo, PipelineVk*, ComputePipelineDescHasher> m_computePipelineCache;

		// VkPipeline cache
		VkPipelineCache m_vkCache;
	};
}