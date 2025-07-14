
#include "render_graph_vk.h"

#include "device_context_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/cache_utils.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/render_graph_type_converter.h"
#include "utils/sanity.h"

namespace PHX
{
	static const char* s_pReservedBackbufferName = "INTERNAL_backbuffer";
	static constexpr u32 s_invalidRenderPassIndex = U32_MAX;

	static u64 HashResourceDesc(const ResourceDesc& desc)
	{
		// Only hash the resourceType and data pointer. That's all we should look at to determine
		// whether two resources are the same
		size_t seed = 0;
		HashCombine(seed, desc.resourceType);
		HashCombine(seed, desc.data);
		return static_cast<u64>(seed);
	}

	static ATTACHMENT_TYPE CalculateAttachmentType(ITexture* pTexture)
	{
		AspectTypeFlags aspectFlags = pTexture->GetAspectFlags();
		switch (aspectFlags)
		{
		case ASPECT_TYPE_FLAG_COLOR:                              return ATTACHMENT_TYPE::COLOR;
		case (ASPECT_TYPE_FLAG_DEPTH | ASPECT_TYPE_FLAG_STENCIL): return ATTACHMENT_TYPE::DEPTH_STENCIL;
		case ASPECT_TYPE_FLAG_DEPTH:                              return ATTACHMENT_TYPE::DEPTH;
		case ASPECT_TYPE_FLAG_STENCIL:                            return ATTACHMENT_TYPE::STENCIL;
		}

		LogError("Failed to calculate attachment type from texture's aspect flags. No valid combination was found for aspect flags %u", aspectFlags);
		return ATTACHMENT_TYPE::INVALID;
	}

	static VkImageLayout CalculateLayoutForInputImage(ATTACHMENT_TYPE attachmentType)
	{
		switch (attachmentType)
		{
		case ATTACHMENT_TYPE::COLOR:         return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case ATTACHMENT_TYPE::DEPTH:         return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
		case ATTACHMENT_TYPE::DEPTH_STENCIL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		case ATTACHMENT_TYPE::STENCIL:       return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
		case ATTACHMENT_TYPE::RESOLVE:       return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Is this right?
		default:
		{
			break;
		}
		}

		LogWarning("Failed to calculate layout for input image with attachment type %u. Defaulting to general layout", static_cast<u32>(attachmentType));
		return VK_IMAGE_LAYOUT_GENERAL;
	}

	size_t ResourceDescHasher::operator()(const ResourceDesc& desc) const
	{

	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	RenderPassVk::RenderPassVk(const char* name, BIND_POINT bindPoint, u32 index, RegisterResourceCallbackFn registerResourceCallback) : 
		m_bindPoint(bindPoint), m_registerResourceCallback(registerResourceCallback), m_index(index)
	{
		ASSERT_MSG(m_registerResourceCallback != nullptr, "Register resource callback is null");

		m_name = HashCRC32(name);

#if defined(PHX_DEBUG)
		m_debugName = name;
#endif
	}

	RenderPassVk::~RenderPassVk()
	{
		m_inputResources.reset();
		m_outputResources.reset();
	}

	void RenderPassVk::SetTextureInput(ITexture* pTexture)
	{
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::INPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = CalculateAttachmentType(pTexture);
		desc.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::LOAD;

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferInput(IBuffer* pBuffer)
	{
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pBuffer;
		desc.io = RESOURCE_IO::INPUT;
		desc.resourceType = RESOURCE_TYPE::BUFFER;

		// Not a texture resource
		desc.attachmentType = ATTACHMENT_TYPE::INVALID;
		desc.storeOp = ATTACHMENT_STORE_OP::INVALID;
		desc.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_inputResources.set(resourceIndex);
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

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetColorOutput(ITexture* pTexture)
	{
		ResourceDesc desc{};
		desc.name = nullptr; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::COLOR;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthOutput(ITexture* pTexture)
	{
		ResourceDesc desc{};
		desc.name = "out_depth_tex"; // TODO
		desc.data = pTexture;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::TEXTURE;
		desc.attachmentType = ATTACHMENT_TYPE::DEPTH;
		desc.storeOp = ATTACHMENT_STORE_OP::STORE;
		desc.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
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

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
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

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
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

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferOutput(IBuffer* pBuffer)
	{
		ResourceDesc desc{};
		desc.name = nullptr;
		desc.data = pBuffer;
		desc.io = RESOURCE_IO::OUTPUT;
		desc.resourceType = RESOURCE_TYPE::BUFFER;

		// Not a texture resource
		desc.attachmentType = ATTACHMENT_TYPE::INVALID;
		desc.storeOp = ATTACHMENT_STORE_OP::INVALID;
		desc.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(desc);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc)
	{
		graphicsDesc = graphicsPipelineDesc;
	}

	void RenderPassVk::SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc)
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

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice) : m_frameIndex(0), m_pReservedBackbufferNameCRC(HashCRC32(s_pReservedBackbufferName)), m_deviceContexts()
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to initialize render graph. Render device is null!");
			return;
		}

		m_renderDevice = pRenderDevice;

		const u32 framesInFlight = m_renderDevice->GetFramesInFlight();
		m_deviceContexts.reserve(framesInFlight);

		for (u32 i = 0; i < framesInFlight; i++)
		{
			IDeviceContext* pDeviceContext = nullptr;
			STATUS_CODE res = m_renderDevice->AllocateDeviceContext({}, &pDeviceContext);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to construct render graph. Device context creation failed!");
				return;
			}

			DeviceContextVk* pDeviceContextVk = static_cast<DeviceContextVk*>(pDeviceContext);
			ASSERT_PTR(pDeviceContextVk);
			m_deviceContexts.push_back(pDeviceContextVk);
		}
	}

	RenderGraphVk::~RenderGraphVk()
	{
		for (DeviceContextVk* deviceContextVk : m_deviceContexts)
		{
			IDeviceContext* pDeviceContext = static_cast<IDeviceContext*>(deviceContextVk);
			m_renderDevice->DeallocateDeviceContext(&pDeviceContext);
		}
		m_deviceContexts.clear();
	}

	STATUS_CODE RenderGraphVk::BeginFrame(ISwapChain* pSwapChain)
	{
		if (pSwapChain == nullptr)
		{
			LogError("Failed to begin frame. Swap chain is null!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(pSwapChain);
		ASSERT_PTR(swapChainVk);

		DeviceContextVk* pDeviceContext = GetDeviceContext();
		res = pDeviceContext->BeginFrame(swapChainVk, m_frameIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame. Device context could not begin frame!");
			return res;
		}

		return res;
	}

	STATUS_CODE RenderGraphVk::EndFrame(ISwapChain* pSwapChain)
	{
		if (pSwapChain == nullptr)
		{
			LogError("Failed to end frame. Swap chain is null!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(pSwapChain);
		ASSERT_PTR(swapChainVk);

		DeviceContextVk* pDeviceContext = GetDeviceContext();
		res = pDeviceContext->EndFrame(m_frameIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame. Device context could not flush!");
			return res;
		}

		// Now that all the work has been done for the current frame, move onto the next one
		m_frameIndex = (m_frameIndex + 1) % m_renderDevice->GetFramesInFlight();

		m_registeredRenderPasses.clear();
		m_registeredResources.clear();
		m_resourceDescCache.clear();

		return res;
	}

	IRenderPass* RenderGraphVk::RegisterPass(const char* passName, BIND_POINT bindPoint)
	{
		const u32 passIndex = static_cast<u32>(m_registeredRenderPasses.size());

		auto registerResourceFuncPtr = std::bind(&RenderGraphVk::RegisterResource, this, std::placeholders::_1);
		m_registeredRenderPasses.emplace_back(passName, bindPoint, passIndex, registerResourceFuncPtr);
		return &m_registeredRenderPasses.back();
	}

	STATUS_CODE RenderGraphVk::Bake(ISwapChain* pSwapChain, ClearValues* pClearColors, u32 clearColorCount)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(pSwapChain);
		ASSERT_PTR(swapChainVk);

		// Create the render graph tree using the following steps:
		// 1. Find the render pass that writes to the back-buffer
		u32 backbufferRPIndex = FindBackBufferRenderPassIndex();
		if (backbufferRPIndex == s_invalidRenderPassIndex)
		{
			LogError("Failed to bake render graph. No render pass writes to backbuffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		RenderPassVk& backbufferRP = m_registeredRenderPasses[backbufferRPIndex];
		
		// 2. Once that render pass is found, build render graph by looking at inputs and working up recursively (using breadth-first search)
		BuildDependencyTree(backbufferRPIndex);

		// 3. [TRIMMING] Accumulate all contributing render passes into a separate container for the render graph. This is done so that
		//               all non-contributing passes are indirectly trimmed
		// TODO

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO
		
		// Now that the render graph has been generated, run through it and perform the following steps for each render pass:
		// 1. Declare the resources that will get used in the device context
		// 2. Insert resource barriers and/or perform layout transitions as necessary
		// 3. Call the execute callback and pass in the device context
		

		// TODO - Simply dealing with the backbuffer pass for now
		// Transition the backbuffer resource to PRESENT layout
		//ResourceDesc& backbufferResourceDesc = backbufferRP.m_outputResources.at(0);
		//if (backbufferResourceDesc.resourceType == RESOURCE_TYPE::TEXTURE)
		//{
		//	ITexture* backbufferResource = static_cast<ITexture*>(backbufferResourceDesc.data);
		//}

		// for(auto& rp : validRenderPasses)

		//		Get or create render pass (should refer to internal cache)
		VkRenderPass renderPassVk = CreateRenderPass(backbufferRP);
		
		//		Get or create framebuffer from render device (should refer to internal cache)
		const bool isBackbuffer = true;
		FramebufferVk* pFramebuffer = CreateFramebuffer(backbufferRP, renderPassVk, isBackbuffer);

		//		Get or create pipeline from render device (should refer to internal cache)
		PipelineVk* pPipeline = CreatePipeline(backbufferRP, renderPassVk);

		DeviceContextVk* pDeviceContext = GetDeviceContext();

		// Look at all input resources and make sure they're in the correct layout
		//for (const ResourceDesc& inputResource : backbufferRP->m_inputResources)
		//{
		//	if (inputResource.resourceType != RESOURCE_TYPE::TEXTURE)
		//	{
		//		continue;
		//	}

		//	VkImageLayout destLayout = CalculateLayoutForInputImage(inputResource.attachmentType);
		//	TextureVk* textureVk = static_cast<TextureVk*>(inputResource.data);

		//	// Issue the commands for image layout barriers
		//	pDeviceContext->TransitionImageLayout(textureVk, destLayout);
		//}

		//FlushSyncData preRPSyncData{}; // No sync for now
		//res = pDeviceContext->Flush(QUEUE_TYPE::TRANSFER, preRPSyncData);
		//if (res != STATUS_CODE::SUCCESS)
		//{
		//	LogError("Failed to bake render graph. Device context could not flush image layout transitions!");
		//	return res;
		//}
		
		res = pDeviceContext->BeginRenderPass(renderPassVk, pFramebuffer, pClearColors, clearColorCount);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render pass. Device context could not begin render pass!");
			return res;
		}

		// Call the main render pass execution callback
		backbufferRP.m_execCallback(pDeviceContext, pPipeline);

		// Flush any transfer operations requested in the execution callback
		//FlushSyncData midRPSyncData{}; // No sync for now
		//res = pDeviceContext->Flush(QUEUE_TYPE::TRANSFER, midRPSyncData);
		//if (res != STATUS_CODE::SUCCESS)
		//{
		//	LogError("Failed to bake render graph. Device context could not flush transfer operations!");
		//	return res;
		//}

		res = pDeviceContext->EndRenderPass();
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to bake render graph. Device context could not end render pass!");
			return res;
		}

		return res;
	}

	VkRenderPass RenderGraphVk::CreateRenderPass(const RenderPassVk& renderPass)
	{
		RenderPassDescription renderPassDesc{};
		renderPassDesc.attachments.reserve(renderPass.m_outputResources.size());
		renderPassDesc.subpasses.reserve(1); // TODO - Support multiple subpasses

		SubpassDescription subpassDesc{};
		subpassDesc.bindPoint = RG_UTILS::ConvertBindPoint(renderPass.m_bindPoint);
		subpassDesc.srcStageMask = 0;
		subpassDesc.dstStageMask = 0;
		subpassDesc.srcAccessMask = 0;
		subpassDesc.dstAccessMask = 0;

		for (u32 i = 0; i < m_registeredResources.size(); i++)
		{
			// Only consider the render pass's output resources
			if (!renderPass.m_outputResources.test(i))
			{
				continue;
			}

			const ResourceDesc& outputResource = m_registeredResources[i];
			if (outputResource.resourceType != RESOURCE_TYPE::TEXTURE)
			{
				continue;
			}

			AttachmentDescription attDesc{};
			attDesc.pTexture = static_cast<TextureVk*>(outputResource.data);
			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // TODO

			// TODO - Since we're only dealing with 1 render pass for now, I'll assume that
			//        the special backbuffer resource will be used as the present resource and
			//        everything else is a depth buffer resource (since this is only for output resources)
			if (HashCRC32(outputResource.name) == m_pReservedBackbufferNameCRC)
			{
				attDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			else
			{
				attDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			
			switch (outputResource.attachmentType)
			{
			case ATTACHMENT_TYPE::COLOR:
			{
				attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				subpassDesc.srcAccessMask |= VK_ACCESS_NONE;
				subpassDesc.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				subpassDesc.colorAttachmentIndices.push_back(i);
				break;
			}
			case ATTACHMENT_TYPE::DEPTH:
			{
				attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO - Switch to DEPTH_ONLY in Vulkan version 1.2?
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::STENCIL:
			{
				attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // TODO - Switch to STENCIL_ONLY in Vulkan version 1.2?
				attDesc.stencilLoadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.stencilStoreOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::DEPTH_STENCIL:
			{
				// TODO - Should this be considered a stencil or regular load/store op?
				attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				subpassDesc.depthStencilAttachmentIndex = i;
				break;
			}
			case ATTACHMENT_TYPE::RESOLVE:
			{
				attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				attDesc.loadOp = ATT_UTILS::ConvertLoadOp(outputResource.loadOp);
				attDesc.storeOp = ATT_UTILS::ConvertStoreOp(outputResource.storeOp);

				subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				subpassDesc.srcAccessMask |= VK_ACCESS_NONE;
				subpassDesc.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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

	FramebufferVk* RenderGraphVk::CreateFramebuffer(const RenderPassVk& renderPass, VkRenderPass renderPassVk, bool isBackBuffer)
	{
		std::vector<FramebufferAttachmentDesc> attachments;
		attachments.reserve(renderPass.m_outputResources.size());

		u32 maxWidth = 0;
		u32 maxHeight = 0;
		for (u32 i = 0; i < m_registeredResources.size(); i++)
		{
			// Only consider the render pass's output resources
			if (!renderPass.m_outputResources.test(i))
			{
				continue;
			}

			ResourceDesc& outputResource = m_registeredResources[i];
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
		framebufferCI.isBackbuffer = isBackBuffer;
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

	u8 RenderGraphVk::RegisterResource(const ResourceDesc& resourceDesc)
	{
		u8 resourceIndex = U8_MAX;

		// See if the same resource has already been registered
		auto iter = m_resourceDescCache.find(resourceDesc);
		if (iter == m_resourceDescCache.end())
		{
			// Couldn't find a matching resource, so create a new one
			resourceIndex = static_cast<u8>(m_registeredResources.size());
			m_registeredResources.push_back(resourceDesc);

			m_resourceDescCache.insert({resourceDesc, resourceIndex});
		}
		else
		{
			// Found existing resource!
			resourceIndex = static_cast<u8>(iter->second);
		}

		return resourceIndex;
	}

	DeviceContextVk* RenderGraphVk::GetDeviceContext() const
	{
		return m_deviceContexts[m_frameIndex];
	}

	u32 RenderGraphVk::FindBackBufferRenderPassIndex()
	{
		u32 backbufferRPIndex = s_invalidRenderPassIndex;

		for (u32 i = 0; i < m_registeredRenderPasses.size(); i++)
		{
			// Check if any of the render pass's output resources are the backbuffer
			const RenderPassVk& renderPass = m_registeredRenderPasses.at(i);
			for (u32 j = 0; j < m_registeredResources.size(); j++)
			{
				const ResourceDesc& outputResource = m_registeredResources[j];
				if (!renderPass.m_outputResources.test(j))
				{
					continue;
				}

				if (outputResource.name == nullptr)
				{
					continue;
				}

				// TODO - Optimize. We probably want to store the CRC rather than the raw string
				const CRC32 outputResourceCRC = HashCRC32(outputResource.name);
				if (outputResourceCRC == m_pReservedBackbufferNameCRC)
				{
					backbufferRPIndex = i;
					break;
				}
			}

			if (backbufferRPIndex != s_invalidRenderPassIndex)
			{
				// Found backbuffer render pass index!
				break;
			}
		}

		return backbufferRPIndex;
	}

	void RenderGraphVk::BuildDependencyTree(u32 renderPassIndex)
	{
		RenderPassVk& currentRenderPass = m_registeredRenderPasses[renderPassIndex];

		// Base case - return if pass has no inputs (or read resources)
		if (currentRenderPass.m_inputResources.none())
		{
			return;
		}

		// Starting off with the backbuffer render pass, recursively find all other passes
		// which write to the input resources
		for (u32 i = 0; i < static_cast<u32>(m_registeredResources.size()); i++)
		{
			const auto& currInputs = currentRenderPass.m_inputResources;
			if (!currInputs.test(i))
			{
				continue;
			}

			for (u32 j = 0; j < static_cast<u32>(m_registeredRenderPasses.size()); j++)
			{
				RenderPassVk& potentialWriteRP = m_registeredRenderPasses[j];
				bool isDependencyRP = ((potentialWriteRP.m_outputResources & currInputs) != 0);
				if (isDependencyRP)
				{
					// Found a dependency. This pass writes to one or more of the current pass's inputs,
					// so store the dependency in the render pass node
					currentRenderPass.m_renderPassDependencies.set(renderPassIndex);
					BuildDependencyTree(i);
				}
			}
		}
	}
}