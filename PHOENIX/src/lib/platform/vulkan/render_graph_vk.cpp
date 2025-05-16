
#include "render_graph_vk.h"

#include "device_context_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/render_graph_type_converter.h"
#include "utils/sanity.h"

namespace PHX
{
	static const char* s_pReservedBackbufferName = "INTERNAL_backbuffer";

	//static RenderPassDescription BuildRenderPassDescription(const FramebufferDescription& info)
	//{
	//	RenderPassDescription rpDesc{};

	//	rpDesc.attachments.resize(info.attachmentCount);
	//	rpDesc.subpasses.resize(1); // Just using 1 subpass for now

	//	SubpassDescription& subpassDesc = rpDesc.subpasses.at(0);

	//	subpassDesc.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Does compute use framebuffers at all?
	//	subpassDesc.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // TODO - optimize
	//	subpassDesc.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // TODO - optimize
	//	subpassDesc.dstAccessMask = VK_ACCESS_NONE;

	//	for (u32 i = 0; i < info.attachmentCount; i++)
	//	{
	//		FramebufferAttachmentDesc& framebufferAttDesc = info.pAttachments[i];
	//		AttachmentDescription& attDesc = rpDesc.attachments.at(i);

	//		attDesc.pTexture = dynamic_cast<TextureVk*>(framebufferAttDesc.pTexture);

	//		VkAttachmentLoadOp loadOp = ATT_UTILS::ConvertLoadOp(framebufferAttDesc.loadOp);
	//		VkAttachmentStoreOp storeOp = ATT_UTILS::ConvertStoreOp(framebufferAttDesc.storeOp);

	//		switch (framebufferAttDesc.type)
	//		{
	//		case ATTACHMENT_TYPE::COLOR:
	//		{
	//			attDesc.loadOp = loadOp;
	//			attDesc.storeOp = storeOp;

	//			// How inefficient is this?
	//			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//			attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	//			attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//			subpassDesc.colorAttachmentIndices.push_back(i);
	//			break;
	//		}
	//		case ATTACHMENT_TYPE::DEPTH:
	//		{
	//			attDesc.loadOp = loadOp;
	//			attDesc.storeOp = storeOp;

	//			// How inefficient is this?
	//			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//			attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	//			attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

	//			if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
	//			{
	//				LogWarning("Attempting to assign depth attachment to framebuffer more than once! Depth attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
	//			}
	//			else
	//			{
	//				subpassDesc.depthStencilAttachmentIndex = i;
	//			}

	//			break;
	//		}
	//		case ATTACHMENT_TYPE::STENCIL:
	//		{
	//			attDesc.stencilLoadOp = loadOp;
	//			attDesc.stencilStoreOp = storeOp;

	//			// How inefficient is this?
	//			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//			attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	//			attDesc.layout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;

	//			if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
	//			{
	//				LogWarning("Attempting to assign stencil attachment to framebuffer more than once! Stencil attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
	//			}
	//			else
	//			{
	//				subpassDesc.depthStencilAttachmentIndex = i;
	//			}

	//			break;
	//		}
	//		case ATTACHMENT_TYPE::DEPTH_STENCIL:
	//		{
	//			attDesc.stencilLoadOp = loadOp;
	//			attDesc.stencilStoreOp = storeOp;

	//			// How inefficient is this?
	//			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//			attDesc.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	//			attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	//			if (subpassDesc.depthStencilAttachmentIndex != U32_MAX)
	//			{
	//				LogWarning("Attempting to assign depth-stencil attachment to framebuffer more than once! Depth-stencil attachment is already assigned to index %u", subpassDesc.depthStencilAttachmentIndex);
	//			}
	//			else
	//			{
	//				subpassDesc.depthStencilAttachmentIndex = i;
	//			}

	//			break;
	//		}
	//		case ATTACHMENT_TYPE::RESOLVE:
	//		{
	//			attDesc.loadOp = loadOp;
	//			attDesc.storeOp = storeOp;

	//			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//			attDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Assuming it'll be presented right after resolving
	//			attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//			if (subpassDesc.resolveAttachmentIndex != U32_MAX)
	//			{
	//				LogWarning("Attempting to assign resolve attachment to framebuffer more than once! Resolve attachment is already assigned to index %u", subpassDesc.resolveAttachmentIndex);
	//			}
	//			else
	//			{
	//				subpassDesc.resolveAttachmentIndex = i;
	//			}

	//			break;
	//		}
	//		}
	//	}

	//	return rpDesc;
	//}

	RenderPassVk::RenderPassVk(const char* name, BIND_POINT bindPoint) : m_bindPoint(bindPoint)
	{
		m_name = HashCRC32(name);

#if defined(PHX_DEBUG)
		m_debugName = name;
#endif
	}

	RenderPassVk::~RenderPassVk()
	{
		m_inputResources.clear();
		m_outputResources.clear();
	}

	void RenderPassVk::SetTextureInput(ITexture* pTexture)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::INPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::COLOR;
		desc.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::LOAD;
		m_inputResources.push_back(desc);
	}

	void RenderPassVk::SetBufferInput(IBuffer* pBuffer)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pBuffer;
		desc.io = RESOURCE_IO::INPUT;
		desc.resourceType = RESOURCE_TYPE::BUFFER;

		// Not a texture resource
		desc.attachmentType = ATTACHMENT_TYPE::INVALID;
		desc.storeOp = ATTACHMENT_STORE_OP::INVALID;
		desc.loadOp = ATTACHMENT_LOAD_OP::INVALID;
		m_inputResources.push_back(desc);
	}

	void RenderPassVk::SetUniformInput(IUniformCollection* pUniformCollection)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pUniformCollection;
		desc.io = RESOURCE_IO::INPUT;
		desc.resourceType = RESOURCE_TYPE::UNIFORM;

		// Not a texture resource
		desc.attachmentType = ATTACHMENT_TYPE::INVALID;
		desc.storeOp = ATTACHMENT_STORE_OP::INVALID;
		desc.loadOp = ATTACHMENT_LOAD_OP::INVALID;
		m_inputResources.push_back(desc);
	}

	void RenderPassVk::SetColorOutput(ITexture* pTexture)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::COLOR;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;
		m_outputResources.push_back(desc);
	}

	void RenderPassVk::SetDepthStencilOutput(ITexture* pTexture)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::DEPTH_STENCIL;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;
		m_outputResources.push_back(desc);
	}

	void RenderPassVk::SetResolveOutput(ITexture* pTexture)
	{
		TODO();
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::RESOLVE;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;
		m_outputResources.push_back(desc);
	}

	void RenderPassVk::SetBackbufferOutput(ITexture* pTexture)
	{
		ResourceDesc desc{};
		desc.name = s_pReservedBackbufferName;
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::COLOR;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		m_outputResources.push_back(desc);
	}

	void RenderPassVk::SetPipeline(const GraphicsPipelineDesc& graphicsPipelineDesc)
	{
		graphicsDesc = graphicsPipelineDesc;
	}

	void RenderPassVk::SetPipeline(const ComputePipelineDesc& computePipelineDesc)
	{
		computeDesc = computePipelineDesc;
	}

	void RenderPassVk::SetExecuteCallback(ExecuteRenderPassCallbackFn callback)
	{
		if (!callback)
		{
			LogError("Failed to set execute callback. Callback parameter is null!");
			return;
		}

		m_execCallback = callback;
	}

	//--------------------------------------------------------------------------------------------

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice) : m_frameIndex(0), m_pReservedBackbufferNameCRC(HashCRC32(s_pReservedBackbufferName))
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

	void RenderGraphVk::Reset()
	{
		// TODO - Clean up memory?
		m_registeredRenderPasses.clear();
		m_registeredResources.clear();
	}

	IRenderPass* RenderGraphVk::RegisterPass(const char* passName, BIND_POINT bindPoint)
	{
		m_registeredRenderPasses.emplace_back(passName, bindPoint);
		return &m_registeredRenderPasses.back();
	}

	STATUS_CODE RenderGraphVk::Bake(ISwapChain* pSwapChain, ClearValues* pClearColors, u32 clearColorCount)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(pSwapChain);
		ASSERT_PTR(swapChainVk);

		// Create the render graph tree using the following steps:
		// 1. Find the render pass that writes to the back-buffer

		int backbufferRPIndex = -1;
		for (int i = 0; i < m_registeredRenderPasses.size(); i++)
		{
			auto& renderPass = m_registeredRenderPasses.at(i);
			// Check if any of the render pass's output resources are the backbuffer
			for (const auto& outputResource : renderPass.m_outputResources)
			{
				// TODO - Optimize. We probably want to store the CRC rather than the raw string
				const CRC32 outputResourceCRC = HashCRC32(outputResource.name);
				if (outputResourceCRC == m_pReservedBackbufferNameCRC)
				{
					backbufferRPIndex = i;
					break;
				}
			}

			if (backbufferRPIndex != -1)
			{
				// Found backbuffer render pass index!
				break;
			}
		}

		if (backbufferRPIndex == -1)
		{
			LogError("Failed to bake render graph! No registered render pass writes to the backbuffer");
			return STATUS_CODE::ERR_API;
		}
		
		// 2. Once that render pass is found, build render graph by looking at inputs and working up recursively (using breadth-first search)
		// TODO - Simply include the backbuffer pass in the render graph for now
		RenderPassVk& backbufferRP = m_registeredRenderPasses.at(backbufferRPIndex);

		// 3. [TRIMMING] Accumulate all contributing render passes into a separate container for the render graph. This is done so that
		//               all non-contributing passes are indirectly trimmed
		// TODO

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO
		
		// Now that the render graph has been generated, run through it and perform the following steps for each render pass:
		// 1. Create (or get?) a device context
		// 2. Declare the resources that will get used in the device context
		// 3. Insert resource barriers and/or perform layout transitions as necessary
		// 4. Call the execute callback and pass in the device context
		IDeviceContext* pDeviceContext = nullptr;
		if (m_renderDevice->AllocateDeviceContext({}, &pDeviceContext) != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render graph! Device context creation failed");
			return STATUS_CODE::ERR_INTERNAL;
		}

		DeviceContextVk* pDeviceContextVk = static_cast<DeviceContextVk*>(pDeviceContext);
		ASSERT_PTR(pDeviceContextVk);

		// TODO - Simply dealing with the backbuffer pass for now
		// Transition the backbuffer resource to PRESENT layout
		ASSERT_MSG(backbufferRP.m_outputResources.size() == 1, "Backbuffer render pass doesn't have exactly 1 output resource!"); // TEMP
		//ResourceDesc& backbufferResourceDesc = backbufferRP.m_outputResources.at(0);
		//if (backbufferResourceDesc.resourceType == RESOURCE_TYPE::TEXTURE)
		//{
		//	ITexture* backbufferResource = static_cast<ITexture*>(backbufferResourceDesc.data);
		//}

		// Use the device context to start/end the frame and all the render passes
		res = pDeviceContextVk->BeginFrame(swapChainVk, m_frameIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render graph. Device context could not begin frame!");
			return res;
		}

		// for(auto& rp : validRenderPasses)

		//		Get or create render pass (should refer to internal cache)
		VkRenderPass renderPassVk = CreateRenderPass(backbufferRP);
		
		//		Get or create framebuffer from render device (should refer to internal cache)
		FramebufferVk* pFramebuffer = CreateFramebuffer(backbufferRP, renderPassVk);

		//		Get or create pipeline from render device (should refer to internal cache)
		PipelineVk* pPipeline = CreatePipeline(backbufferRP, renderPassVk);
		
		res = pDeviceContextVk->BeginRenderPass(renderPassVk, pFramebuffer, pClearColors, clearColorCount);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render pass. Device context could not begin render pass!");
			return res;
		}

		backbufferRP.m_execCallback(pDeviceContextVk, pPipeline);

		res = pDeviceContextVk->EndRenderPass();
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render graph. Device context could not end render pass!");
			return res;
		}

		res = pDeviceContextVk->Flush(swapChainVk, m_frameIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render graph. Device context could not flush!");
			return res;
		}

		// Now that all the work has been done for the current frame, move onto the next one
		m_frameIndex = (m_frameIndex + 1) % m_renderDevice->GetFramesInFlight();

		return res;
	}

	VkRenderPass RenderGraphVk::CreateRenderPass(const RenderPassVk& renderPass)
	{
		RenderPassDescription renderPassDesc{};
		renderPassDesc.attachments.reserve(renderPass.m_outputResources.size());
		renderPassDesc.subpasses.reserve(1); // TODO - Support multiple subpasses

		SubpassDescription subpassDesc{};
		subpassDesc.bindPoint = RG_UTILS::ConvertBindPoint(renderPass.m_bindPoint);
		subpassDesc.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // TODO - optimize
		subpassDesc.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // TODO - optimize
		subpassDesc.dstAccessMask = VK_ACCESS_NONE;

		for (u32 i = 0; i < renderPass.m_outputResources.size(); i++)
		{
			const ResourceDesc& outputResource = renderPass.m_outputResources[i];
			if (outputResource.resourceType != RESOURCE_TYPE::TEXTURE)
			{
				continue;
			}

			AttachmentDescription attDesc{};
			attDesc.pTexture = static_cast<TextureVk*>(outputResource.data);
			attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // TODO
			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // TODO
			attDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // TODO
			
			switch (outputResource.attachmentType)
			{
			case ATTACHMENT_TYPE::COLOR:
			{
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.colorAttachmentIndices.push_back(i);
				break;
			}
			case ATTACHMENT_TYPE::DEPTH:
			{
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::STENCIL:
			{
				attDesc.stencilLoadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.stencilStoreOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::DEPTH_STENCIL:
			{
				// TODO - Should this be considered a stencil or regular load/store op?
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::RESOLVE:
			{
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				ASSERT_MSG(subpassDesc.resolveAttachmentIndex == -1, "Already assigned the resolve attachment index!");
				subpassDesc.resolveAttachmentIndex = i;
				break;
			}
			}

			renderPassDesc.attachments.push_back(attDesc);
		}

		renderPassDesc.subpasses.push_back(subpassDesc); // TODO - Support multiple subpasses

		VkRenderPass renderPassVk = m_renderDevice->CreateRenderPass(renderPassDesc);
		return renderPassVk;
	}

	FramebufferVk* RenderGraphVk::CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk)
	{
		std::vector<FramebufferAttachmentDesc> attachments;
		attachments.reserve(renderPass.m_outputResources.size());

		u32 maxWidth = 0;
		u32 maxHeight = 0;
		for (auto& outputResource : renderPass.m_outputResources)
		{
			if (outputResource.resourceType != RESOURCE_TYPE::TEXTURE)
			{
				continue;
			}

			TextureVk* pAttachmentTex = static_cast<TextureVk*>(outputResource.data);
			if (pAttachmentTex == nullptr)
			{
#if defined(PHX_DEBUG)
				LogError("Failed to create framebuffer for render pass \"%s\"! Output texture resource does not have a valid texture pointer", renderPass.m_debugName);
#else
				// TODO - Maybe create crc database and convert crc to string for log message?
				LogError("Failed to create framebuffer for render pass \"%X\"! Output texture resource does not have a valid texture pointer", renderPass.m_name);
#endif
				return nullptr;
			}

			FramebufferAttachmentDesc desc;
			desc.pTexture = static_cast<TextureVk*>(outputResource.data);
			desc.mipTarget = 0;
			desc.type = outputResource.attachmentType;
			desc.storeOp = outputResource.storeOp;
			desc.loadOp = outputResource.loadOp;

			attachments.push_back(desc);

			maxWidth = Max(maxWidth, pAttachmentTex->GetWidth());
			maxHeight = Max(maxHeight, pAttachmentTex->GetHeight());
		}

		FramebufferDescription framebufferCI{};
		framebufferCI.width = maxWidth;
		framebufferCI.height = maxHeight;
		framebufferCI.layers = 1;
		framebufferCI.pAttachments = attachments.data();
		framebufferCI.attachmentCount = static_cast<u32>(attachments.size());
		framebufferCI.renderPass = renderPassVk;
		FramebufferVk* pFramebuffer = m_renderDevice->CreateFramebuffer(framebufferCI);
		return pFramebuffer;
	}

	PipelineVk* RenderGraphVk::CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk)
	{
		PipelineVk* pipeline = nullptr;

		BIND_POINT renderPassBindPoint = renderPass.m_bindPoint;
		switch (renderPassBindPoint)
		{
		case BIND_POINT::GRAPHICS:
		{
			pipeline = m_renderDevice->CreateGraphicsPipeline(renderPass.graphicsDesc, renderPassVk);
			break;
		}
		case BIND_POINT::COMPUTE:
		{
			pipeline = m_renderDevice->CreateComputePipeline(renderPass.computeDesc);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Unknown bind point!");
			break;
		}
		}

		return pipeline;
	}
}