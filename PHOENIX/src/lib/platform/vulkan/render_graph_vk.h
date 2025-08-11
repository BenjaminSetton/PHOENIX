#pragma once

#include <bitset>
#include <functional>
#include <vector>

#include "framebuffer_vk.h"
#include "PHX/interface/render_graph.h"
#include "pipeline_vk.h"
#include "utils/crc32.h"
#include "utils/render_graph_utils.h"

namespace PHX
{
	static constexpr u8 MAX_REGISTERED_RESOURCES = U8_MAX;

	// Forward declarations
	class DeviceContextVk;
	class RenderDeviceVk;
	class RenderGraphVk;
	class RenderPassVk;

	// Callback used by the render pass class to register resources into
	// the render graph. The render graph owns the resources, and once
	// they're registered this function will return the resource index
	// that should then be cached in the render pass's input/output bitsets.
	// Parameters are the data (void*), resource type (RESOURCE_TYPE), and 
	// resource usage (ResourceUsage)
	typedef std::function<u8(void*, RESOURCE_TYPE, const ResourceUsage&)> RegisterResourceCallbackFn;

	// Called for every render pass touched when traversing the dependency tree
	typedef std::function<void(const RenderPassVk&)> TraverseDependenciesCallbackFn;

	// Called for every resource touched while traversing a resource bitset
	typedef std::function<void(const RenderResource&)> TraverseResourceCallbackFn;

	typedef std::bitset<MAX_REGISTERED_RESOURCES> ResourceIndexBitset;

	struct Barrier
	{
		VkPipelineStageFlags srcStageMask;
		VkPipelineStageFlags dstStageMask;
		VkAccessFlags srcAccessMask;
		VkAccessFlags dstAccessMask;

		VkImageLayout oldLayout; // Only for images, ignored for buffers
		VkImageLayout newLayout; // Only for images, ignored for buffers
	};

	struct DependencyInfo
	{
		RenderPassVk* renderPass = nullptr;
		ResourceIndexBitset resources;
	};

	class RenderPassVk : public IRenderPass
	{
	public:

		friend class RenderGraphVk;

		explicit RenderPassVk(const char* name, BIND_POINT bindPoint, u32 index, RegisterResourceCallbackFn registerResourceCallback);
		~RenderPassVk() override;

		// Inputs
		void SetTextureInput(ITexture* pTexture) override;
		void SetBufferInput(IBuffer* pBuffer) override; // Not sure if I want to keep this
		void SetUniformInput(IUniformCollection* pUniformCollection) override; // Not sure if I want to keep this

		// Outputs
		void SetColorOutput(ITexture* pTexture) override;
		void SetDepthOutput(ITexture* pTexture) override;
		void SetDepthStencilOutput(ITexture* pTexture) override;
		void SetResolveOutput(ITexture* pTexture) override;
		void SetBackbufferOutput(ITexture* pTexture) override;
		void SetBufferOutput(IBuffer* pBuffer) override;

		// Pipeline
		void SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc) override;
		void SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc) override;

		// Callbacks
		void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) override;

	private:

		CRC32 m_name;

#if defined(PHX_DEBUG)
		const char* m_debugName;
#endif

		ResourceIndexBitset m_inputResources;					// Physical resource indices which this pass reads from
		ResourceIndexBitset m_outputResources;					// Physical resources indices which this pass writes to
		ExecuteRenderPassCallbackFn m_execCallback;				// Execution callback called by the render graph if all validation checks are passed
		RegisterResourceCallbackFn m_registerResourceCallback;	// Callback used to register resources into the render graph
		u32 m_index;											// Index of the render pass in the context of the render graph

		// TODO - Consider using union?
		GraphicsPipelineDesc graphicsDesc;
		ComputePipelineDesc computeDesc;
		BIND_POINT m_bindPoint;

		std::vector<DependencyInfo> m_dependencyInfos;

		// Maps a physical resource ID to it's barrier. These barriers guard the inputs to this render
		// pass against all dependencies' outputs depending on usage. All src flags correspond to a dependent 
		// resource, and all dst flags correspond to an input resource in this pass. For any given barrier, 
		// the src/dst flags are all related to the same physical resource, but the usage is different between 
		// the dependency and this pass.
		std::unordered_map<u64, Barrier> m_inputBarriers;

		// Maps a physical resource ID to it's barrier. Same concept as m_inputBarriers above, except
		// that this holds barrier information for all outputs in this pass. This is useful when creating
		// render passes, since the finalLayout of an image must be specified during subpass creation.
		std::unordered_map<u64, Barrier> m_outputBarriers;
	};

	class RenderGraphVk : public IRenderGraph
	{
	public:

		RenderGraphVk(RenderDeviceVk* pRenderDevice);
		~RenderGraphVk() override;

		STATUS_CODE BeginFrame(ISwapChain* pSwapChain) override;
		STATUS_CODE EndFrame() override;
		IRenderPass* RegisterPass(const char* passName, BIND_POINT bindPoint) override;
		STATUS_CODE Bake(ClearValues* pClearColors, u32 clearColorCount) override;
		STATUS_CODE GenerateVisualization(const char* fileName, bool generateIfUnique) override;

	private:

		VkRenderPass CreateRenderPass(const RenderPassVk& renderPass);
		FramebufferVk* CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk, bool isBackBuffer);
		PipelineVk* CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk);
		u8 RegisterResource(void* data, RESOURCE_TYPE type, const ResourceUsage& usage);

		DeviceContextVk* GetDeviceContext() const;
		u32 FindBackBufferRenderPassIndex();

		void BuildDependencyTree(u32 finalPassIndex);
		void FindActivePasses(u32 finalPassIndex, std::vector<u32>& out_activeRenderPasses);
		void CalculateResourceBarriers(const std::vector<u32>& activeRenderPasses);

		STATUS_CODE InsertResourceBarriers(const RenderPassVk& renderPass);

		void TraverseDependencyTree(u32 renderPassIndex, TraverseDependenciesCallbackFn callback);

		void TraverseResources(const ResourceIndexBitset& resourceBitset, TraverseResourceCallbackFn callback) const;
		void TraverseRenderPassInputs(u32 renderPassIndex, TraverseResourceCallbackFn callback) const;
		void TraverseRenderPassOutputs(u32 renderPassIndex, TraverseResourceCallbackFn callback) const;

		// Returns ResourceUsage* if found, nullptr otherwise
		const ResourceUsage* GetResourceUsageFromPass(const RenderPassVk& renderPass, u64 resourceID) const;
		const RenderResource* GetPhysicalResource(u64 resourceID) const;

		// Updates the render pass' textures to whatever layout they were implicitly transitioned to
		// by the render pass dependency. This information is stored in the output barriers
		void UpdateTextureLayouts(u32 renderPassIndex);

	private:

		std::vector<RenderPassVk> m_registeredRenderPasses;
		std::vector<ResourceUsage> m_resourceUsages;
		std::vector<RenderResource> m_physicalResources;
		RenderDeviceVk* m_renderDevice;

		// One device context per frame in flight
		std::vector<DeviceContextVk*> m_deviceContexts;

		u32 m_frameIndex;

		const CRC32 m_reservedBackbufferNameCRC;
		const CRC32 m_reservedDepthBufferNameCRC;
	};
}