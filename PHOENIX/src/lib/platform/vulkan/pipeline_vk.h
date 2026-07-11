#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/pipeline_desc.h"
#include "PHX/types/status_code.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;
	struct BufferData;

	// Pipelines have no interface type!
	class PipelineVk
	{
	public:

		PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo);
		PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo);
		PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const RayTracingPipelineDesc& createInfo);
		~PipelineVk();

		VkPipeline GetPipeline() const;
		VkPipelineLayout GetLayout() const;
		VkPipelineBindPoint GetBindPoint() const;

		const VkStridedDeviceAddressRegionKHR* GetRayGenSBTRegion() const;
		const VkStridedDeviceAddressRegionKHR* GetMissSBTRegion() const;
		const VkStridedDeviceAddressRegionKHR* GetHitSBTRegion() const;
		const VkStridedDeviceAddressRegionKHR* GetCallableSBTRegion() const;

	private:

		STATUS_CODE CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo);
		STATUS_CODE CreateComputePipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo);
		STATUS_CODE CreateRayTracingPipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const RayTracingPipelineDesc& createInfo);

		STATUS_CODE VerifyCreateInfo(const GraphicsPipelineDesc& createInfo);
		STATUS_CODE VerifyCreateInfo(const ComputePipelineDesc& createInfo);
		STATUS_CODE VerifyCreateInfo(const RayTracingPipelineDesc& createInfo);

		VkPipelineLayout CreatePipelineLayout(VkDevice logicalDevice, UniformCollectionHandle uniformCollection);

		bool IsRayTracingShaderStage(SHADER_STAGE stage) const;

	private:

		RenderDeviceVk* m_pRenderDevice;

		VkPipeline m_pipeline;
		VkPipelineLayout m_layout;
		VkPipelineBindPoint m_bindPoint;

		BufferData* m_sbt;
		VkStridedDeviceAddressRegionKHR m_rayGenSBTRegion;
		VkStridedDeviceAddressRegionKHR m_missSBTRegion;
		VkStridedDeviceAddressRegionKHR m_hitSBTRegion;
		VkStridedDeviceAddressRegionKHR m_callableSBTRegion;
	};
}