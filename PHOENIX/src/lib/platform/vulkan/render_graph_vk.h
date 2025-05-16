#pragma once

#include <bitset>
#include <vector>

#include "framebuffer_vk.h"
#include "PHX/interface/render_graph.h"
#include "pipeline_vk.h"
#include "utils/crc32.h"
#include "utils/render_graph_utils.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;
	class RenderGraphVk;

	class RenderPassVk : public IRenderPass
	{
	public:

		friend class RenderGraphVk;

		explicit RenderPassVk(const char* name, BIND_POINT bindPoint);
		~RenderPassVk() override;

		// Inputs
		void SetTextureInput(ITexture* pTexture) override;
		void SetBufferInput(IBuffer* pBuffer) override; // Not sure if I want to keep this
		void SetUniformInput(IUniformCollection* pUniformCollection) override; // Not sure if I want to keep this

		// Outputs
		void SetColorOutput(ITexture* pTexture) override;
		void SetDepthStencilOutput(ITexture* pTexture) override;
		void SetResolveOutput(ITexture* pTexture) override;
		void SetBackbufferOutput(ITexture* pTexture) override;

		// Pipeline
		void SetPipeline(const GraphicsPipelineDesc& graphicsPipelineDesc) override;
		void SetPipeline(const ComputePipelineDesc& computePipelineDesc) override;

		// Callbacks
		void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) override;

	private:

		CRC32 m_name;

#if defined(PHX_DEBUG)
		const char* m_debugName;
#endif

		std::vector<ResourceDesc> m_inputResources;
		std::vector<ResourceDesc> m_outputResources;
		ExecuteRenderPassCallbackFn m_execCallback;

		// TODO - Consider using union?
		GraphicsPipelineDesc graphicsDesc;
		ComputePipelineDesc computeDesc;
		BIND_POINT m_bindPoint;
	};

	class RenderGraphVk : public IRenderGraph
	{
	public:

		RenderGraphVk(RenderDeviceVk* pRenderDevice);
		~RenderGraphVk() override;

		void Reset() override;
		IRenderPass* RegisterPass(const char* passName, BIND_POINT bindPoint) override;
		STATUS_CODE Bake(ISwapChain* pSwapChain, ClearValues* pClearColors, u32 clearColorCount) override;

	private:

		VkRenderPass CreateRenderPass(const RenderPassVk& renderPass);
		FramebufferVk* CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk);
		PipelineVk* CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk);

	private:

		std::vector<RenderPassVk> m_registeredRenderPasses;
		std::vector<ResourceDesc> m_registeredResources;
		RenderDeviceVk* m_renderDevice;

		u32 m_frameIndex;

		const CRC32 m_pReservedBackbufferNameCRC;
	};
}