#pragma once

#include <bitset>
#include <functional>
#include <variant>
#include <vector>

#include "BSL/crc32.h"
#include "core/handle/handle_list.h"
#include "core/interface_types/render_graph_interface.h"
#include "framebuffer_vk.h"
#include "PHX/types/pass_timing.h"
#include "pipeline_vk.h"
#include "utils/render_graph_utils.h"

namespace PHX
{
	using ResourceIndex = u16;

	TECHDEBT("Kinda arbitrary. Find better solution?");
	static constexpr ResourceIndex MAX_REGISTERED_RESOURCES = 512;

	// Forward declarations
	class AccelerationStructureVk;
	class DeviceContextVk;
	class RenderDeviceVk;
	class RenderGraphVk;
	class RenderPassVk;
	class TextureVk;
	class BufferVk;


	// Callback used by the render pass class to register resources into
	// the render graph. The render graph owns the resources, and once
	// they're registered this function will return the resource index
	// that should then be cached in the render pass's input/output bitsets.
	// Parameters are the handle (Handle), resource type (RESOURCE_TYPE), and 
	// resource usage (ResourceUsage)
	typedef std::function<ResourceIndex(Handle, RESOURCE_TYPE, const ResourceUsage&)> RegisterResourceCallbackFn;

	// Called for every render pass touched when traversing the dependency tree
	typedef std::function<void(const RenderPassVk&)> TraverseDependenciesCallbackFn;

	// Called for every resource touched while traversing a resource bitset
	typedef std::function<void(const RenderResource&)> TraverseResourceCallbackFn;

	typedef std::bitset<MAX_REGISTERED_RESOURCES> ResourceIndexBitset;

	struct Barrier
	{
		Barrier() = default;
		Barrier(RenderResource res) : physicalResource(res)
		{
		}

		RenderResource physicalResource;

		// Access hazards
		VkPipelineStageFlags srcStageMask	= VK_PIPELINE_STAGE_NONE;
		VkPipelineStageFlags dstStageMask	= VK_PIPELINE_STAGE_NONE;
		VkAccessFlags srcAccessMask			= VK_ACCESS_NONE;
		VkAccessFlags dstAccessMask			= VK_ACCESS_NONE;

		// Texture layout
		VkImageLayout oldLayout		= VK_IMAGE_LAYOUT_UNDEFINED; // Only for images, ignored for buffers
		VkImageLayout currentLayout	= VK_IMAGE_LAYOUT_UNDEFINED; // Only for images, ignored for buffers
		VkImageLayout nextLayout	= VK_IMAGE_LAYOUT_UNDEFINED; // Only for images, ignored for buffers

		bool IsValid()
		{
			return !(srcStageMask	== VK_PIPELINE_STAGE_NONE		&&
					dstStageMask	== VK_PIPELINE_STAGE_NONE		&&
					srcAccessMask	== VK_ACCESS_NONE				&&
					dstAccessMask	== VK_ACCESS_NONE				&&
					oldLayout		== VK_IMAGE_LAYOUT_UNDEFINED	&&
					currentLayout	== VK_IMAGE_LAYOUT_UNDEFINED	&&
					nextLayout		== VK_IMAGE_LAYOUT_UNDEFINED);
		}
	};

	// Each entry has two pieces of information: the other render pass and a list of resources 
	// which were found to pose access hazards. Dependency information can go both ways
	// (dependent-relative and dependee-relative), so this terminology is purposefully ambiguous
	struct DependencyInfo
	{
		RenderPassVk* renderPass = nullptr;
		ResourceIndexBitset resources;
	};

	enum class TraversalDirection : u8
	{
		FORWARD,
		BACKWARD
	};

	class RenderPassVk : public IRenderPass
	{
	public:

		friend class RenderGraphVk;

		explicit RenderPassVk(const char* name, PASS_TYPE passType, u32 index, RegisterResourceCallbackFn registerResourceCallback);
		~RenderPassVk() override;

		// Inputs
		void SetTextureInput(TextureHandle texture) override;
		void SetBufferInput(BufferHandle buffer) override;
		/*[unused]*/ void SetUniformInput(UniformCollectionHandle uniformCollection) override;
		void SetAccelerationStructureInput(AccelerationStructureHandle accelerationStructure) override;

		// Outputs
		void SetTextureOutput(TextureHandle texture, ATTACHMENT_LOAD_OP loadOp, ATTACHMENT_STORE_OP storeOp, ClearValues clearValue = {}) override;
		void SetColorOutput(TextureHandle texture) override;
		void SetDepthOutput(TextureHandle texture) override;
		/*[unused]*/ void SetDepthStencilOutput(TextureHandle texture) override;
		/*[unused]*/ void SetResolveOutput(TextureHandle texture) override;
		void SetBufferOutput(BufferHandle buffer) override;
		void SetAccelerationStructureOutput(AccelerationStructureHandle accelerationStructure) override;

		// Pipeline
		void SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc) override;
		void SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc) override;
		void SetPipelineDescription(const RayTracingPipelineDesc& rayTracingPipelineDesc) override;

		// Callbacks
		void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) override;

	private:

		const char* m_name;

		ResourceIndexBitset m_readResources;					// Physical resource indices which this pass reads from
		ResourceIndexBitset m_writeResources;					// Physical resources indices which this pass writes to
		ExecuteRenderPassCallbackFn m_execCallback;				// Execution callback called by the render graph if all validation checks are passed
		RegisterResourceCallbackFn m_registerResourceCallback;	// Callback used to register resources into the render graph
		u32 m_index;											// Index of the render pass in the context of the render graph

		std::variant<GraphicsPipelineDesc, ComputePipelineDesc, RayTracingPipelineDesc> m_pipelineDesc;
		PASS_TYPE m_passType;

		// The forward and backward dependency infos, relative to submission order
		// Forward-facing means from submission order onward, and backward essentially
		// starts at the "end of the graph" (backbuffer write) to first submission
		std::vector<DependencyInfo> m_forwardDependencies;
		std::vector<DependencyInfo> m_backwardDependencies;

		// Holds the barriers calculated in CalculateResourceBarriers() that handles both access hazards as well
		// as texture layout transitions
		std::unordered_map<ResourceID, Barrier> m_barriers;
	};

	class RenderGraphVk : public IRenderGraph
	{
	public:

		RenderGraphVk(RenderDeviceVk* pRenderDevice);
		~RenderGraphVk() override;

		STATUS_CODE BeginFrame(SwapChainHandle swapChain) override;
		STATUS_CODE EndFrame(SwapChainHandle swapChain) override;
		STATUS_CODE RegisterPass(const char* passName, PASS_TYPE passType, RenderPassHandle& renderPass) override;
		STATUS_CODE Bake(SwapChainHandle swapChain) override;
		u32 GetFrameNumber() const override;
		const Metrics& GetMetrics() const override;
		STATUS_CODE GenerateVisualization(const char* fileName, bool generateIfUnique) override;

		IDeviceContext* GetCurrentDeviceContext() override;
		DeviceContextHandle GetCurrentDeviceContextHandle() override;

		// Handles
		void* ResolveHandle(const Handle& handle) override;
		void IncrementHandleRefCount(const Handle& handle) override;
		void DecrementHandleRefCount(const Handle& handle) override;

	private:

		VkRenderPass CreateRenderPass(const RenderPassVk& renderPass);
		FramebufferVk* CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk, bool isBackBuffer);
		PipelineVk* CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk);

		ResourceIndex RegisterResource(Handle resource, RESOURCE_TYPE type, const ResourceUsage& usage);
		RenderPassVk* RegisterPass_Internal(const char* passName, PASS_TYPE passType);

		// Returns the index of the pass that owns presentation: the last (highest submission index)
		// active pass that writes to the current swapchain image. Earlier swapchain writers are pulled
		// in as dependencies via the write-after-write hazard on the shared image.
		u32 FindPresentRenderPassIndex(ResourceID presentResID);

		void BuildDependencyTree(u32 renderPassIndex);
		void CalculateActivePasses(u32 presentRenderPassIndex, std::vector<u32>& out_activePasses);
		void AdjustResourceUsages(const std::vector<u32>& activePassIndices);

		STATUS_CODE CalculateResourceBarriers(RenderPassVk& renderPass);
		Barrier BuildSyncResourceBarrier(const RenderResource& physicalResource, const ResourceUsage& srcUsage, const ResourceUsage& dstUsage);
		Barrier BuildLayoutResourceBarrier(const RenderResource& physicalResource, const ResourceUsage* prevUsage, const ResourceUsage* currUsage, const ResourceUsage* nextUsage);
		STATUS_CODE InsertResourceBarriers(const RenderPassVk& renderPass);

		void TraverseDependencyTree(u32 renderPassIndex, TraversalDirection dir, TraverseDependenciesCallbackFn callback);
		void TraverseResources(const ResourceIndexBitset& resourceBitset, TraverseResourceCallbackFn callback) const;

		// Searches for a resource usage for the given physical resource made by passIndex, at the provided offset.
		// An offset of 0 represents the "current usage" (at passIndex), while -1/+1 represent the "previous usage" 
		// and "next usage" in the chain respectively.
		// Returns the ResourceUsage when successful, or nullptr if something went wrong or the usage does not exist
		const ResourceUsage* GetResourceUsageFromPass(ResourceID resourceId, u32 passIndex, i32 offset);

		TextureVk* ResolveTexture(const RenderResource& resource);
		BufferVk* ResolveBuffer(const RenderResource& resource);
		AccelerationStructureVk* ResolveAccelerationStructure(const RenderResource& resource);
		const char* GetResourceName(const RenderResource& resource);

		// Returns a hash of the state of the render graph in the current frame
		u64 HashState() const;

		void CallExecutionCallback(const RenderPassVk& renderPass, const DeviceContextHandle& deviceContext);

		void WriteBeginTimestamp(DeviceContextVk* pDeviceContext, const RenderPassVk& renderPass);
		void WriteEndTimestamp(DeviceContextVk* pDeviceContext);

	private:

		HandleList<RenderPassVk> m_registeredRenderPasses;
		std::unordered_map<ResourceID, std::vector<ResourceUsage>> m_resourcesUsageChain;
		std::vector<RenderResource> m_physicalResources;
		RenderDeviceVk* m_pRenderDevice;

		std::vector<DeviceContextHandle> m_deviceContextHandles;

		u64 m_currentFrameGraphHash;

		// Stores the unique hashes for which a visualization has been generated
		std::unordered_map<u64, bool> m_uniqueVisualizationHashes;

		u32 m_frameInFlightIndex;
		u32 m_frameNumber;

		ResourceID m_presentResID;

		// Metrics
		mutable Metrics m_metrics;
	};
}