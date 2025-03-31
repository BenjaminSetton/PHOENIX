#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"

namespace PHX
{
	typedef void(*BuildRenderPassCallbackFn)(IDeviceContext* pContext);

	class IRenderPass
	{
	public:

		virtual ~IRenderPass() { }

		// Inputs
		virtual void SetTextureInput(ITexture* pTexture) = 0;
		virtual void SetBufferInput(IBuffer* pBuffer) = 0;							// Not sure if I want to keep this
		virtual void SetUniformInput(IUniformCollection* pUniformCollection) = 0;	// Not sure if I want to keep this
			 
		// Outputs
		virtual void SetColorTarget(ITexture* pTexture) = 0;
		virtual void SetDepthStencilTarget(ITexture* pTexture) = 0;
		virtual void SetResolveTarget(ITexture* pTexture) = 0;

		// Callbacks
		virtual void Build(BuildRenderPassCallbackFn callback) = 0;
	};

	class IRenderGraph
	{
	public:

		virtual ~IRenderGraph() { }

		virtual IRenderPass* AddPass(const char* passName/*, u32 flags_TODO*/) = 0;
		virtual void Bake() = 0;
	};
}