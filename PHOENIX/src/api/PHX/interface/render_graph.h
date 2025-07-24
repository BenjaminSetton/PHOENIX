#pragma once

#include <functional>

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/clear_color.h"

namespace PHX
{
	typedef std::function<void(IDeviceContext* pContext, IPipeline* pPipeline)> ExecuteRenderPassCallbackFn;

	enum class BIND_POINT : u8
	{
		GRAPHICS = 0,
		COMPUTE,
		TRANSFER
	};

	class IRenderPass
	{
	public:

		virtual ~IRenderPass() { }

		// Inputs
		virtual void SetTextureInput(ITexture* pTexture) = 0;
		virtual void SetBufferInput(IBuffer* pBuffer) = 0;							// Not sure if I want to keep this
		virtual void SetUniformInput(IUniformCollection* pUniformCollection) = 0;	// Not sure if I want to keep this
			 
		// Outputs
		virtual void SetColorOutput(ITexture* pTexture) = 0;
		virtual void SetDepthOutput(ITexture* pTexture) = 0;
		virtual void SetDepthStencilOutput(ITexture* pTexture) = 0;
		virtual void SetResolveOutput(ITexture* pTexture) = 0;
		virtual void SetBackbufferOutput(ITexture* pTexture) = 0;
		virtual void SetBufferOutput(IBuffer* pBuffer) = 0;

		// Pipeline data
		virtual void SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc) = 0;
		virtual void SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc) = 0;

		// Callbacks
		virtual void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) = 0;
	};

	class IRenderGraph
	{
	public:

		virtual ~IRenderGraph() { }

		virtual STATUS_CODE BeginFrame(ISwapChain* pSwapChain) = 0;
		virtual STATUS_CODE EndFrame() = 0;
		virtual IRenderPass* RegisterPass(const char* passName, BIND_POINT bindPoint) = 0;
		virtual STATUS_CODE Bake(ClearValues* pClearColors, u32 clearColorCount) = 0;

		// Generates a visualization of the render graph by creating a .dot file. This file can then be
		// opened with a graph visualization tool such as GraphViz to examine the graph structure. If
		// the "generateIfUnique" parameter is set to true, a new file will be written out only if the
		// structure of the render graph is unique from any other previously-generated visualization. If
		// set to false, it will generate a new visualization every time the graph is different from the
		// one generated immediately before
		virtual STATUS_CODE GenerateVisualization(const char* fileName, bool generateIfUnique = true) = 0;
	};
}