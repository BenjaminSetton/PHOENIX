#pragma once

#include <functional>

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"

namespace PHX
{
	typedef std::function<void(IDeviceContext* pContext, IPipeline* pPipeline)> ExecuteRenderPassCallbackFn;

	enum class BIND_POINT : u8
	{
		GRAPHICS = 0,
		COMPUTE
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
		virtual void SetDepthStencilOutput(ITexture* pTexture) = 0;
		virtual void SetResolveOutput(ITexture* pTexture) = 0;
		virtual void SetBackbufferOutput(ITexture* pTexture) = 0;

		// Pipeline data
		virtual void SetPipeline(const GraphicsPipelineDesc& graphicsPipelineDesc) = 0;
		virtual void SetPipeline(const ComputePipelineDesc& computePipelineDesc) = 0;

		// Callbacks
		virtual void SetExecuteCallback(ExecuteRenderPassCallbackFn callback) = 0;
	};

	class IRenderGraph
	{
	public:

		virtual ~IRenderGraph() { }

		virtual void Reset() = 0;
		virtual IRenderPass* RegisterPass(const char* passName, BIND_POINT bindPoint) = 0;
		virtual STATUS_CODE Bake(ISwapChain* pSwapChain, ClearValues* pClearColors, u32 clearColorCount) = 0;
	};
}