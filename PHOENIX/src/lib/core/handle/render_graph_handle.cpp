
#include "PHX/interface/render_device.h"
#include "PHX/interface/render_graph.h"

#include "core/handle/handle_utils.h"
#include "utils/sanity.h"

namespace PHX
{
	RenderPassHandle::RenderPassHandle() : Handle(HANDLE_TYPE::RENDER_PASS)
	{
	}

	RenderPassHandle::RenderPassHandle(const Handle& base) : Handle(base)
	{
	}

	RenderPassHandle::~RenderPassHandle()
	{
	}

	RenderPassHandle::RenderPassHandle(const RenderPassHandle& other) : Handle(other)
	{
	}

	RenderPassHandle& RenderPassHandle::operator=(const RenderPassHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	RenderPassHandle::RenderPassHandle(RenderPassHandle&& other) noexcept : Handle(std::move(other))
	{
	}

	void RenderPassHandle::SetTextureInput(TextureHandle texture)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetTextureInput(texture);
		}
	}

	void RenderPassHandle::SetBufferInput(BufferHandle buffer)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetBufferInput(buffer);
		}
	}

	void RenderPassHandle::SetUniformInput(UniformCollectionHandle uniformCollection)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetUniformInput(uniformCollection);
		}
	}

	void RenderPassHandle::SetColorOutput(TextureHandle texture)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetColorOutput(texture);
		}
	}

	void RenderPassHandle::SetDepthOutput(TextureHandle texture)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetDepthOutput(texture);
		}
	}

	void RenderPassHandle::SetDepthStencilOutput(TextureHandle texture)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetDepthStencilOutput(texture);
		}
	}

	void RenderPassHandle::SetResolveOutput(TextureHandle texture)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetResolveOutput(texture);
		}
	}

	void RenderPassHandle::SetTextureOutput(TextureHandle texture, ATTACHMENT_LOAD_OP loadOp, ATTACHMENT_STORE_OP storeOp, ClearValues clearValue)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetTextureOutput(texture, loadOp, storeOp, clearValue);
		}
	}

	void RenderPassHandle::SetBufferOutput(BufferHandle buffer)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetBufferOutput(buffer);
		}
	}

	void RenderPassHandle::SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetPipelineDescription(graphicsPipelineDesc);
		}
	}

	void RenderPassHandle::SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetPipelineDescription(computePipelineDesc);
		}
	}

	void RenderPassHandle::SetExecuteCallback(ExecuteRenderPassCallbackFn callback)
	{
		IRenderPass* pPass = HANDLE_UTILS::ResolveHandle(*this);
		if (pPass != nullptr)
		{
			return pPass->SetExecuteCallback(callback);
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////

	RenderGraphHandle::RenderGraphHandle() : Handle(HANDLE_TYPE::RENDER_GRAPH)
	{
	}

	RenderGraphHandle::RenderGraphHandle(const Handle& base) : Handle(base)
	{
	}

	RenderGraphHandle::~RenderGraphHandle()
	{
	}

	RenderGraphHandle::RenderGraphHandle(const RenderGraphHandle& other) : Handle(other)
	{
	}

	RenderGraphHandle& RenderGraphHandle::operator=(const RenderGraphHandle& other)
	{
		if (this == &other)
		{
			return *this;
		}

		Handle::operator=(other);
		return *this;
	}

	RenderGraphHandle::RenderGraphHandle(RenderGraphHandle&& other) noexcept : Handle(std::move(other))
	{
	}
	
	STATUS_CODE RenderGraphHandle::BeginFrame(SwapChainHandle swapChain)
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->BeginFrame(swapChain);
		}

		ASSERT_ALWAYS("Failed to begin frame. Could not resolve render graph handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderGraphHandle::EndFrame()
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->EndFrame();
		}

		ASSERT_ALWAYS("Failed to end frame. Could not resolve render graph handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderGraphHandle::RegisterPass(const char* passName, BIND_POINT bindPoint, RenderPassHandle& renderPass)
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->RegisterPass(passName, bindPoint, renderPass);
		}

		ASSERT_ALWAYS("Failed to begin frame. Could not resolve render graph handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	STATUS_CODE RenderGraphHandle::Bake(SwapChainHandle swapChain)
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->Bake(swapChain);
		}

		ASSERT_ALWAYS("Failed to bake. Could not resolve render graph handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}

	u32 RenderGraphHandle::GetFrameNumber() const
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->GetFrameNumber();
		}

		ASSERT_ALWAYS("Failed to get frame number. Could not resolve render graph handle!");
		return U32_MAX;
	}

	STATUS_CODE RenderGraphHandle::GenerateVisualization(const char* fileName, bool generateIfUnique)
	{
		IRenderGraph* pGraph = HANDLE_UTILS::ResolveHandle(*this);
		if (pGraph != nullptr)
		{
			return pGraph->GenerateVisualization(fileName, generateIfUnique);
		}

		ASSERT_ALWAYS("Failed to generate visualization. Could not resolve render graph handle!");
		return STATUS_CODE::ERR_INTERNAL;
	}
}