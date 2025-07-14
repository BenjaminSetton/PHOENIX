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
	static constexpr u32 MAX_REGISTERED_RESOURCES = 256;

	// Callback used by the render pass class to register resources into
	// the render graph. The render graph owns the resources, and once
	// they're registered this function will return the resource index
	// that should then be cached in the render pass's input/output bitsets
	typedef std::function<u8(const ResourceDesc&)> RegisterResourceCallbackFn;

	// Forward declarations
	class RenderDeviceVk;
	class RenderGraphVk;
	class DeviceContextVk;

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

		std::bitset<MAX_REGISTERED_RESOURCES> m_inputResources;
		std::bitset<MAX_REGISTERED_RESOURCES> m_outputResources;
		ExecuteRenderPassCallbackFn m_execCallback;
		RegisterResourceCallbackFn m_registerResourceCallback;
		u32 m_index;

		// TODO - Consider using union?
		GraphicsPipelineDesc graphicsDesc;
		ComputePipelineDesc computeDesc;
		BIND_POINT m_bindPoint;

		std::bitset<MAX_REGISTERED_RESOURCES> m_renderPassDependencies;
	};

	class RenderGraphVk : public IRenderGraph
	{
	public:

		RenderGraphVk(RenderDeviceVk* pRenderDevice);
		~RenderGraphVk() override;

		STATUS_CODE BeginFrame(ISwapChain* pSwapChain) override;
		STATUS_CODE EndFrame(ISwapChain* pSwapChain) override;
		IRenderPass* RegisterPass(const char* passName, BIND_POINT bindPoint) override;
		STATUS_CODE Bake(ISwapChain* pSwapChain, ClearValues* pClearColors, u32 clearColorCount) override;

	private:

		VkRenderPass CreateRenderPass(const RenderPassVk& renderPass);
		FramebufferVk* CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk, bool isBackBuffer);
		PipelineVk* CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk);
		u8 RegisterResource(const ResourceDesc& resourceDesc);

		DeviceContextVk* GetDeviceContext() const;
		u32 FindBackBufferRenderPassIndex();

		void BuildDependencyTree(u32 backbufferRPIndex);

	private:

		std::vector<RenderPassVk> m_registeredRenderPasses;
		std::vector<ResourceDesc> m_registeredResources;
		std::unordered_map<u64, u32> m_resourceDescCache; // TODO - Aliased resouce map/cache? We must determine when a resource is being aliased (e.g. one pass uses it as texture input, while another uses it as texture output)
		RenderDeviceVk* m_renderDevice;

		// One device context per frame in flight
		std::vector<DeviceContextVk*> m_deviceContexts;

		u32 m_frameIndex;

		const CRC32 m_pReservedBackbufferNameCRC;
	};
}