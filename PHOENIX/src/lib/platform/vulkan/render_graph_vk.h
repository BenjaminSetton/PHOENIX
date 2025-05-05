#pragma once

#include <bitset>
//#include <unordered_map>
#include <vector>

#include "PHX/interface/render_graph.h"
#include "utils/crc32.h"
#include "utils/render_graph_utils.h"
#include "framebuffer_vk.h"

namespace PHX
{
	//static constexpr u32 MAX_RENDER_GRAPH_RESOURCES = 256;

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

		// Callbacks
		void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) override;

	private:

		CRC32 m_name;

#if defined(PHX_DEBUG)
		const char* m_debugName;
#endif

		std::vector<ResourceDesc> m_inputResources;
		std::vector<ResourceDesc> m_outputResources;
		//std::unordered_map<CRC32, ResourceDesc> m_nameToResource;
		ExecuteRenderPassCallbackFn m_execCallback;
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

	private:

		std::vector<RenderPassVk> m_registeredRenderPasses;
		std::vector<ResourceDesc> m_registeredResources;
		RenderDeviceVk* m_renderDevice;
	};
}