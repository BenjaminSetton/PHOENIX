#pragma once

#include <functional>

#include "PHX/interface/buffer.h"
#include "PHX/interface/device_context.h"
#include "PHX/interface/handle.h"
#include "PHX/interface/swap_chain.h"
#include "PHX/interface/texture.h"
#include "PHX/interface/uniform.h"
#include "PHX/types/attachment_desc.h"
#include "PHX/types/clear_color.h"
#include "PHX/types/pipeline_desc.h"

namespace PHX
{
	typedef std::function<void(DeviceContextHandle deviceContext)> ExecuteRenderPassCallbackFn;

	enum class BIND_POINT : u8
	{
		GRAPHICS = 0,
		COMPUTE,
		TRANSFER
	};

	struct RenderPassHandle : public Handle
	{
		DECLARE_PHX_HANDLE(RenderPassHandle);

		// Inputs
		void SetTextureInput(TextureHandle texture);
		void SetBufferInput(BufferHandle buffer);
		void SetUniformInput(UniformCollectionHandle uniformCollection); // Not sure if I want to keep this

		// Outputs
		// SetTextureOutput is the generic texture-write entry point. The attachment type (color/depth/
		// stencil/resolve) is inferred from the texture's aspect flags. Swapchain images are written
		// through this same path - there is nothing special about them other than being presented.
		void SetTextureOutput(TextureHandle texture, ATTACHMENT_LOAD_OP loadOp, ATTACHMENT_STORE_OP storeOp, ClearValues clearValue = {});
		void SetColorOutput(TextureHandle texture);
		void SetDepthOutput(TextureHandle texture);
		void SetDepthStencilOutput(TextureHandle texture);
		void SetResolveOutput(TextureHandle texture);
		void SetBufferOutput(BufferHandle buffer);

		// Pipeline data
		void SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc);
		void SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc);

		// Callbacks
		void SetExecuteCallback(ExecuteRenderPassCallbackFn callback);
	};

	struct RenderGraphHandle : public Handle
	{
		DECLARE_PHX_HANDLE(RenderGraphHandle);

		STATUS_CODE BeginFrame(SwapChainHandle swapChain);
		STATUS_CODE EndFrame();
		STATUS_CODE RegisterPass(const char* passName, BIND_POINT bindPoint, RenderPassHandle& renderPass);

		// Bakes and executes the render graph. The swap chain is passed in so the graph can identify
		// which passes write to the current swapchain image (the present target). Any number of passes
		// may write to it; the last one (in registration order) owns presentation.
		STATUS_CODE Bake(SwapChainHandle swapChain);

		u32 GetFrameNumber() const;

		// Generates a visualization of the render graph by creating a .dot file. This file can then be
		// opened with a graph visualization tool such as GraphViz to examine the graph structure. If
		// the "generateIfUnique" parameter is set to true, a new file will be written out only if the
		// structure of the render graph is unique from any other previously-generated visualization. If
		// set to false, it will generate a new visualization every time the graph is different from the
		// one generated immediately before
		STATUS_CODE GenerateVisualization(const char* fileName, bool generateIfUnique = true);
	};
}