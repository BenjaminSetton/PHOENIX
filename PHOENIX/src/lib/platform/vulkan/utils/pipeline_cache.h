#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "../pipeline_vk.h"

namespace PHX
{
	struct GraphicsPipelineDescHasher
	{
		size_t operator()(const GraphicsPipelineDesc& desc) const;
	};

	struct ComputePipelineDescHasher
	{
		size_t operator()(const ComputePipelineDesc& desc) const;
	};

	struct RayTracingPipelineDescHasher
	{
		size_t operator()(const RayTracingPipelineDesc& desc) const;
	};

	class PipelineCache
	{
	public:

		explicit PipelineCache(RenderDeviceVk* pRenderDevice);
		~PipelineCache();

		PipelineCache(const PipelineCache& other) = delete;
		PipelineCache& operator=(const PipelineCache& other) = delete;

		// Graphics pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, VkRenderPass renderPass, const GraphicsPipelineDesc& desc);
		PipelineVk* Find(const GraphicsPipelineDesc& desc);
		void Delete(const GraphicsPipelineDesc& desc);

		// Compute pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const ComputePipelineDesc& desc);
		PipelineVk* Find(const ComputePipelineDesc& desc);
		void Delete(const ComputePipelineDesc& desc);

		// Ray tracing pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const RayTracingPipelineDesc& desc);
		PipelineVk* Find(const RayTracingPipelineDesc& desc);
		void Delete(const RayTracingPipelineDesc& desc);

		// Deletes all cached pipelines from all three caches. The VkPipelineCache is preserved
		void Flush();

	private:

		RenderDeviceVk* m_renderDevice;

		// PipelineVk caches
		std::unordered_map<GraphicsPipelineDesc, PipelineVk*, GraphicsPipelineDescHasher> m_graphicsPipelineCache;
		std::unordered_map<ComputePipelineDesc, PipelineVk*, ComputePipelineDescHasher> m_computePipelineCache;
		std::unordered_map<RayTracingPipelineDesc, PipelineVk*, RayTracingPipelineDescHasher> m_rayTracingPipelineCache;

		// VkPipeline cache
		VkPipelineCache m_vkCache;
	};
}