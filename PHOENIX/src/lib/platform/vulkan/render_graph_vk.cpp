
#include "render_graph_vk.h"

#include "utils/logger.h"
#include "utils/sanity.h"

namespace PHX
{
	RenderPassVk::RenderPassVk()
	{
		TODO();
	}

	RenderPassVk::~RenderPassVk()
	{
		TODO();
	}

	void RenderPassVk::SetTextureInput(ITexture* pTexture)
	{
		// TODO - Add extra info for input
		m_texResources.push_back(pTexture);
	}

	void RenderPassVk::SetBufferInput(IBuffer* pBuffer)
	{
		// TODO - Add extra info for input
		m_bufferResources.push_back(pBuffer);
	}

	void RenderPassVk::SetUniformInput(IUniformCollection* pUniformCollection)
	{
		// TODO - Add extra info for input
		m_uniformResources.push_back(pUniformCollection);
	}

	void RenderPassVk::SetColorTarget(ITexture* pTexture)
	{
		TODO();
	}

	void RenderPassVk::SetDepthStencilTarget(ITexture* pTexture)
	{
		TODO();
	}

	void RenderPassVk::SetResolveTarget(ITexture* pTexture)
	{
		TODO();
	}

	void RenderPassVk::Build(BuildRenderPassCallbackFn callback)
	{
		// Validate the render pass
		TODO();
	}

	//--------------------------------------------------------------------------------------------

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to initialize render graph. Render device is null!");
			return;
		}

		m_renderDevice = pRenderDevice;
	}

	RenderGraphVk::~RenderGraphVk()
	{
		TODO();
	}

	IRenderPass* RenderGraphVk::AddPass(const char* passName/*, u32 flags_TODO*/)
	{
		m_renderPasses.emplace_back();
		return &m_renderPasses.back();
	}

	void RenderGraphVk::Bake()
	{
		// Bake process is broken down into 3 steps:
		// 1. Trim unreferenced/superfluous render passes
		// 2. Merge render passes with no overlapping dependencies
		// 3. Insert barriers for resource layout transitions
		TODO();
	}
}