#pragma once

#include <vector>

#include "PHX/interface/render_graph.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class RenderPassVk : public IRenderPass
	{
	public:

		RenderPassVk();
		~RenderPassVk() override;

		// Inputs
		void SetTextureInput(ITexture* pTexture) override;
		void SetBufferInput(IBuffer* pBuffer) override; // Not sure if I want to keep this
		void SetUniformInput(IUniformCollection* pUniformCollection) override; // Not sure if I want to keep this

		// Outputs
		void SetColorTarget(ITexture* pTexture) override;
		void SetDepthStencilTarget(ITexture* pTexture) override;
		void SetResolveTarget(ITexture* pTexture) override;

		// Callbacks
		void Build(BuildRenderPassCallbackFn callback) override;

	private:

		std::vector<ITexture*> m_texResources;
		std::vector<IBuffer*> m_bufferResources;
		std::vector<IUniformCollection*> m_uniformResources;
		BuildRenderPassCallbackFn m_callback;

	};

	class RenderGraphVk : public IRenderGraph
	{
	public:

		RenderGraphVk(RenderDeviceVk* pRenderDevice);
		~RenderGraphVk() override;

		IRenderPass* AddPass(const char* passName/*, u32 flags_TODO*/) override;
		void Bake() override;

	private:

		std::vector<RenderPassVk> m_renderPasses;
		RenderDeviceVk* m_renderDevice;
	};
}