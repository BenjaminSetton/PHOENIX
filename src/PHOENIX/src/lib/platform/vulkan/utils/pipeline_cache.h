#pragma once

#include <unordered_map>
#include <vulkan/vulkan.h>

#include "../pipeline_vk.h"

namespace PHX
{
	using PipelineDescKey = u64;

	PipelineDescKey HashPipelineDesc(const GraphicsPipelineDesc& desc);
	PipelineDescKey HashPipelineDesc(const ComputePipelineDesc& desc);
	PipelineDescKey HashPipelineDesc(const RayTracingPipelineDesc& desc);

	class PipelineCache
	{
	public:

		explicit PipelineCache(RenderDeviceVk* pRenderDevice);
		~PipelineCache();

		PipelineCache(const PipelineCache& other) = delete;
		PipelineCache& operator=(const PipelineCache& other) = delete;

		// Graphics pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, VkRenderPass renderPass, const GraphicsPipelineDesc& desc);
		void Delete(const GraphicsPipelineDesc& desc);

		// Compute pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const ComputePipelineDesc& desc);
		void Delete(const ComputePipelineDesc& desc);

		// Ray tracing pipeline
		PipelineVk* FindOrCreate(RenderDeviceVk* pRenderDevice, const RayTracingPipelineDesc& desc);
		void Delete(const RayTracingPipelineDesc& desc);

		// Deletes all cached pipelines from all three caches. The VkPipelineCache is preserved
		void Flush();

		u32 GetCount() const;

	private:

		PipelineVk* Find_Internal(PipelineDescKey key, const std::unordered_map<PipelineDescKey, PipelineVk*>& cache);

		RenderDeviceVk* m_renderDevice;

		// PipelineVk caches
		std::unordered_map<PipelineDescKey, PipelineVk*> m_graphicsPipelineCache;
		std::unordered_map<PipelineDescKey, PipelineVk*> m_computePipelineCache;
		std::unordered_map<PipelineDescKey, PipelineVk*> m_rayTracingPipelineCache;

		// VkPipeline cache
		VkPipelineCache m_vkCache;
	};
}