
#include <utility> // std::swap

#include "render_graph_vk.h"

#include "buffer_vk.h"
#include "device_context_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/cache_utils.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/render_graph_type_converter.h"
#include "utils/sanity.h"

// Most of the implementation details were taken from the following sources:
// https://poniesandlight.co.uk/reflect/island_rendergraph_1/
//

namespace PHX
{
	static const char* s_pReservedBackbufferName = "INTERNAL_backbuffer";
	static const char* s_pReservedDepthBufferName = "INTERNAL_depthbuffer";
	static constexpr u32 s_invalidRenderPassIndex = U32_MAX;

	static u64 HashResource(void* data, const RESOURCE_TYPE& type)
	{
		size_t seed = 0;
		HashCombine(seed, type);
		HashCombine(seed, data);

		return static_cast<u64>(seed);
	}

	static QUEUE_TYPE ConvertBindPointToQueueType(BIND_POINT bindPoint)
	{
		switch (bindPoint)
		{
		case BIND_POINT::COMPUTE:  return QUEUE_TYPE::COMPUTE;
		case BIND_POINT::GRAPHICS: return QUEUE_TYPE::GRAPHICS;
		case BIND_POINT::TRANSFER: return QUEUE_TYPE::TRANSFER;
		}

		ASSERT_ALWAYS("Failed to convert bind point to queue type!");
		return QUEUE_TYPE::GRAPHICS;
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

	static VkAccessFlags CalculateResourceAccessFlags(const ResourceUsage& usage, const RenderResource& resource, BIND_POINT bindPoint)
	{
		VkAccessFlags flags = 0;
		switch (usage.io)
		{
		case RESOURCE_IO::INPUT:
		{
			if (bindPoint == BIND_POINT::TRANSFER)
			{
				flags |= VK_ACCESS_TRANSFER_READ_BIT;
			}
			else
			{
				switch (resource.type)
				{
				case RESOURCE_TYPE::BUFFER:
				{
					// Determine whether this access is for vertex, index or storage buffer
					BufferVk* pBuffer = static_cast<BufferVk*>(resource.data);
					ASSERT_PTR(pBuffer); // Should never be null, otherwise no point in doing any of this work

					const BUFFER_USAGE bufferUsage = pBuffer->GetUsage();
					switch (bufferUsage)
					{
					case BUFFER_USAGE::VERTEX_BUFFER:
					{
						flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
						break;
					}
					case BUFFER_USAGE::INDEX_BUFFER:
					{
						flags |= VK_ACCESS_INDEX_READ_BIT;
						break;
					}
					case BUFFER_USAGE::STORAGE_BUFFER:
					case BUFFER_USAGE::UNIFORM_BUFFER: // fall-thru
					{
						flags |= VK_ACCESS_SHADER_READ_BIT;
						break;
					}
					case BUFFER_USAGE::INDIRECT_BUFFER:
					{
						TODO(); // No idea what to do here
						break;
					}
					default:
					{
						ASSERT_MSG("Failed to calculate resource access flag. Unknown buffer usage %u!", static_cast<u32>(bufferUsage));
						break;
					}
					}
					break;
				}
				case RESOURCE_TYPE::TEXTURE:
				{
					switch (usage.attachmentType)
					{
					case ATTACHMENT_TYPE::COLOR:
					{
						flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
						break;
					}
					case ATTACHMENT_TYPE::DEPTH:
					case ATTACHMENT_TYPE::STENCIL: // fall-thru
					case ATTACHMENT_TYPE::DEPTH_STENCIL: // fall-thru
					{
						flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
						break;
					}
					case ATTACHMENT_TYPE::RESOLVE:
					{
						// Is an *input* resolve texture a logic error?
						ASSERT_ALWAYS("Cannot calculate access flag for input resolve texture");
						break;
					}
					case ATTACHMENT_TYPE::INVALID:
					{
						// Not sure how we got here in the first place
						ASSERT_ALWAYS("Cannot calculate access flag for input texture with an invalid attachment type!");
						break;
					}
					}
					break;
				}
				case RESOURCE_TYPE::UNIFORM:
				{
					flags |= VK_ACCESS_UNIFORM_READ_BIT;
					break;
				}
				default:
				{
					ASSERT_MSG("Failed to calculate resource access flag. Unknown resource type %u!", static_cast<u32>(resource.type));
					break;
				}
				}
			}
			break;
		}
		case RESOURCE_IO::OUTPUT:
		{
			if (bindPoint == BIND_POINT::TRANSFER)
			{
				flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			else
			{
				switch (resource.type)
				{
				case RESOURCE_TYPE::BUFFER:
				{
					flags |= VK_ACCESS_SHADER_WRITE_BIT;
					break;
				}
				case RESOURCE_TYPE::TEXTURE:
				{
					switch (usage.attachmentType)
					{
					case ATTACHMENT_TYPE::COLOR:
					{
						flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						break;
					}
					case ATTACHMENT_TYPE::DEPTH:
					case ATTACHMENT_TYPE::STENCIL: // fall-thru
					case ATTACHMENT_TYPE::DEPTH_STENCIL: // fall-thru
					{
						flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;
					}
					case ATTACHMENT_TYPE::RESOLVE:
					{
						// Not sure if this is correct
						flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						break;
					}
					case ATTACHMENT_TYPE::INVALID:
					{
						// Not sure how we got here in the first place
						ASSERT_ALWAYS("Cannot calculate access flag for input texture with an invalid attachment type!");
						break;
					}
					}
					break;
				}
				case RESOURCE_TYPE::UNIFORM:
				{
					// Logic error
					ASSERT_ALWAYS("Cannot calculate access flag for an output uniform!");
					break;
				}
				default:
				{
					ASSERT_MSG("Failed to calculate resource access flag. Unknown resource type %u!", static_cast<u32>(resource.type));
					break;
				}
				}
				break;
			}
			break;
		}
		default:
		{
			ASSERT_MSG("Failed to calculate resource access flag. Unknown IO type %u!", static_cast<u32>(usage.io));
			break;
		}
		}

		return flags;
	}

	static VkPipelineStageFlags CalculateResourcePipelineStageFlags(BIND_POINT bindPoint, VkAccessFlags accessFlag, bool isSrcFlag)
	{
		if (accessFlag == 0)
		{
			return (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		}

		VkPipelineStageFlags flags = 0;
		switch (bindPoint)
		{
		case BIND_POINT::GRAPHICS:
		{
			switch (accessFlag)
			{
			case VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT:
			case VK_ACCESS_INDEX_READ_BIT:
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
				break;
			}
			case VK_ACCESS_UNIFORM_READ_BIT:
			{
				// TODO - Determine proper shader stage
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
				break;
			}
			case VK_ACCESS_INPUT_ATTACHMENT_READ_BIT:
			{
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;
			}
			case VK_ACCESS_SHADER_READ_BIT:
			case VK_ACCESS_SHADER_WRITE_BIT:
			{
				// TODO - Determine proper shader stage
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
				break;
			}
			case VK_ACCESS_COLOR_ATTACHMENT_READ_BIT:
			{
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
				break;
			}
			case VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT:
			{
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;
			}
			case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT:
			{
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
				break;
			}
			case VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT:
			{
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
				break;
			}
			case VK_ACCESS_TRANSFER_READ_BIT:
			case VK_ACCESS_TRANSFER_WRITE_BIT:
			{
				ASSERT_ALWAYS("Transfer access flags shouldn't be handled in graphics bind point");
				break;
			}
			case VK_ACCESS_HOST_READ_BIT:
			case VK_ACCESS_HOST_WRITE_BIT:
			{
				flags |= VK_PIPELINE_STAGE_HOST_BIT;
				break;
			}
			case VK_ACCESS_MEMORY_READ_BIT:
			case VK_ACCESS_MEMORY_WRITE_BIT:
			{
				// Not sure if these flags are good, assuming worst-case scenario
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			}

			break;
		}
		case BIND_POINT::COMPUTE:
		{
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		}
		case BIND_POINT::TRANSFER:
		{
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to calculate resource pipeline stage flags. Unknown bind point!");
		}
		}

		return flags;
	}

	static VkImageLayout CalculateResourceImageLayout(const ResourceUsage& usage, BIND_POINT bindPoint)
	{
		switch (usage.io)
		{
		case RESOURCE_IO::INPUT:
		{
			switch (bindPoint)
			{
			case BIND_POINT::GRAPHICS:
			{
				switch (usage.attachmentType)
				{
				case ATTACHMENT_TYPE::DEPTH:
				case ATTACHMENT_TYPE::DEPTH_STENCIL:
				case ATTACHMENT_TYPE::STENCIL:
				{
					return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}
				default:
				{
					return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
				}
			}
			case BIND_POINT::COMPUTE:
			{
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			case BIND_POINT::TRANSFER:
			{
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			default:
			{
				break;
			}
			}
			break;
		}
		case RESOURCE_IO::OUTPUT:
		{
			switch (bindPoint)
			{
			case BIND_POINT::GRAPHICS:
			{
				switch (usage.attachmentType)
				{
				case ATTACHMENT_TYPE::COLOR:
				case ATTACHMENT_TYPE::RESOLVE:
				{
					return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				case ATTACHMENT_TYPE::DEPTH:
				case ATTACHMENT_TYPE::DEPTH_STENCIL: // fall-thru
				case ATTACHMENT_TYPE::STENCIL: // fall-thru
				{
					return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				default:
				{
					break;
				}
				}
			}
			case BIND_POINT::COMPUTE:
			{
				return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
			case BIND_POINT::TRANSFER:
			{
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
			default:
			{
				break;
			}
			}
			break;
		}
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to calculate new image layout. Returning general image layout!");
		return VK_IMAGE_LAYOUT_GENERAL;
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
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;
		usage.attachmentType = CalculateAttachmentType(pTexture);
		usage.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::LOAD;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferInput(IBuffer* pBuffer)
	{
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(pBuffer, RESOURCE_TYPE::BUFFER, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetUniformInput(IUniformCollection* pUniformCollection)
	{
		TODO();
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(pUniformCollection, RESOURCE_TYPE::UNIFORM, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetColorOutput(ITexture* pTexture)
	{
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::COLOR;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthOutput(ITexture* pTexture)
	{
		ResourceUsage usage{};
		usage.name = s_pReservedDepthBufferName; // HACK - Assuming all depth writes are for the depth buffer
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthStencilOutput(ITexture* pTexture)
	{
		TODO();
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH_STENCIL;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetResolveOutput(ITexture* pTexture)
	{
		TODO();
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::RESOLVE;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBackbufferOutput(ITexture* pTexture)
	{
		ResourceUsage usage{};
		usage.name = s_pReservedBackbufferName;
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::COLOR;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(pTexture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferOutput(IBuffer* pBuffer)
	{
		ResourceUsage usage{};
		usage.name = nullptr;
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(pBuffer, RESOURCE_TYPE::BUFFER, usage);
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

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice) : m_deviceContexts(), m_frameIndex(0), 
		m_reservedBackbufferNameCRC(HashCRC32(s_pReservedBackbufferName)), m_reservedDepthBufferNameCRC(HashCRC32(s_pReservedDepthBufferName))
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

	STATUS_CODE RenderGraphVk::EndFrame()
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

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
		m_resourceUsages.clear();
		m_physicalResources.clear();

		return res;
	}

	IRenderPass* RenderGraphVk::RegisterPass(const char* passName, BIND_POINT bindPoint)
	{
		const u32 passIndex = static_cast<u32>(m_registeredRenderPasses.size());

		auto registerResourceFuncPtr = std::bind(&RenderGraphVk::RegisterResource, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_registeredRenderPasses.emplace_back(passName, bindPoint, passIndex, registerResourceFuncPtr);
		return &m_registeredRenderPasses.back();
	}

	STATUS_CODE RenderGraphVk::Bake(ClearValues* pClearColors, u32 clearColorCount)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		// Create the render graph tree using the following steps:
		// 1. Find the render pass that writes to the back-buffer
		u32 finalRPIndex = FindBackBufferRenderPassIndex();
		if (finalRPIndex == s_invalidRenderPassIndex)
		{
			LogError("Failed to bake render graph. No render pass writes to backbuffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		//RenderPassVk& backbufferRP = m_registeredRenderPasses[finalRPIndex];
		
		// 2. Once that render pass is found, build the dependency tree
		BuildDependencyTree(finalRPIndex);

		// 3. [TRIMMING] Accumulate all contributing render passes into a separate container for the render graph. This is done so that
		//               all non-contributing passes are indirectly trimmed
		std::vector<u32> activeRenderPassIndices;
		activeRenderPassIndices.reserve(m_registeredRenderPasses.size());

		FindActivePasses(finalRPIndex, activeRenderPassIndices);

		CalculateResourceBarriers(activeRenderPassIndices);

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO
		
		// Now that the render graph has been generated, run through it and perform the following steps for each render pass:
		// 1. Declare the resources that will get used in the device context
		// 2. Insert resource barriers and/or perform layout transitions as necessary
		// 3. Call the execute callback and pass in the device context
		std::reverse(activeRenderPassIndices.begin(), activeRenderPassIndices.end());

		DeviceContextVk* pDeviceContext = GetDeviceContext();
		for (u32 activeRenderPassIndex : activeRenderPassIndices)
		{
			const RenderPassVk& currRenderPass = m_registeredRenderPasses[activeRenderPassIndex];

			// Before calling execution callback, insert all barriers required by the render pass
			res = InsertResourceBarriers(currRenderPass);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to bake render graph. Could not insert dependency barriers!");
				return res;
			}

			switch (currRenderPass.m_bindPoint)
			{
				case BIND_POINT::GRAPHICS:
				{
					// Get or create render pass (should refer to internal cache)
					VkRenderPass renderPassVk = CreateRenderPass(currRenderPass);
		
					// Get or create framebuffer from render device (should refer to internal cache)
					const bool isBackbuffer = (currRenderPass.m_index == finalRPIndex);
					FramebufferVk* pFramebuffer = CreateFramebuffer(currRenderPass, renderPassVk, isBackbuffer);

					// Get or create pipeline from render device (should refer to internal cache)
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, renderPassVk);

					res = pDeviceContext->BeginRenderPass(renderPassVk, pFramebuffer, pClearColors, clearColorCount);
					if (res != STATUS_CODE::SUCCESS)
					{
						LogError("Failed to bake render pass. Device context could not begin render pass!");
						return res;
					}

					// Call the main render pass execution callback
					currRenderPass.m_execCallback(pDeviceContext, pPipeline);

					res = pDeviceContext->EndRenderPass();
					if (res != STATUS_CODE::SUCCESS)
					{
						LogError("Failed to bake render graph. Device context could not end render pass!");
						return res;
					}

					// Update the layout of the render pass' textures to reflect the implicit 
					// layout transition from the render pass
					UpdateTextureLayouts(activeRenderPassIndex);

					break;
				}
				case BIND_POINT::COMPUTE:
				{
					// Get or create pipeline from render device (should refer to internal cache)
					// NOTE - The render pass isn't used for compute pipeline creation, so it can
					// be ignored by passing in VK_NULL_HANDLE
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, VK_NULL_HANDLE);

					currRenderPass.m_execCallback(pDeviceContext, pPipeline);

					break;
				}
				case BIND_POINT::TRANSFER:
				{
					// NOTE - Transfer-only passes do not use a pipeline
					currRenderPass.m_execCallback(pDeviceContext, nullptr);

					break;
				}
			}
		}

		return res;
	}

	STATUS_CODE RenderGraphVk::GenerateVisualization(const char* fileName, bool generateIfUnique)
	{
		UNUSED(fileName);
		UNUSED(generateIfUnique);
		TODO();

		return STATUS_CODE::SUCCESS;
	}

	VkRenderPass RenderGraphVk::CreateRenderPass(const RenderPassVk& renderPass)
	{
		RenderPassDescription renderPassDesc{};
		renderPassDesc.attachments.reserve(renderPass.m_outputResources.size());
		renderPassDesc.subpasses.reserve(1); // TODO - Support multiple subpasses

		SubpassDescription subpassDesc{};
		subpassDesc.bindPoint = RG_UTILS::ConvertBindPoint(renderPass.m_bindPoint);

		// Incremented every iteration of TraverseRenderPassOutputs below
		u32 localResourceIndex = 0;

		TraverseRenderPassOutputs(renderPass.m_index, [&](const RenderResource& outputResource)
		{
			if (outputResource.type != RESOURCE_TYPE::TEXTURE)
			{
				// Ignore any non-texture output resources
				return;
			}

			TextureVk* pTexture = static_cast<TextureVk*>(outputResource.data);
			ASSERT_PTR(pTexture);

			AttachmentDescription attDesc{};
			attDesc.pTexture = pTexture;

			const ResourceUsage* resourceUsage = GetResourceUsageFromPass(renderPass, outputResource.resourceID);
			if (resourceUsage == nullptr)
			{
				ASSERT_ALWAYS("Failed to create render pass. Render pass uses physical resource but has no usage for it?");
				return;
			}

			// Use the pre-computed barrier information as a sub-pass dependency in this case
			auto iter = renderPass.m_outputBarriers.find(outputResource.resourceID);
			if (iter == renderPass.m_outputBarriers.end())
			{
				// If we can't find any output barriers and the render pass didn't get trimmed, this
				// means that it's the backbuffer pass since no other pass depends on it. 
				// 
				// We'll make some assumptions about what the resource access and stage masks are 
				// in this last pass. Not sure what else to do at the moment

				//attDesc.initialLayout = pTexture->GetLayout();

				//switch (resourceUsage->attachmentType)
				//{
				//case ATTACHMENT_TYPE::COLOR:
				//{
				//	attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				//	attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				//	if (HashCRC32(resourceUsage->name) == m_pReservedBackbufferNameCRC)
				//	{
				//		attDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				//	}

				//	attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
				//	attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

				//	subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				//	subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				//	subpassDesc.srcAccessMask |= VK_ACCESS_NONE;
				//	subpassDesc.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				//	subpassDesc.colorAttachmentIndices.push_back(localResourceIndex);
				//	break;
				//}
				//case ATTACHMENT_TYPE::DEPTH:
				//{
				//	attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				//	attDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				//	attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
				//	attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

				//	subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				//	subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				//	subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				//	subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				//	ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				//	subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
				//	break;
				//}
				//case ATTACHMENT_TYPE::STENCIL:
				//{
				//	attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				//	attDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				//	attDesc.stencilLoadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
				//	attDesc.stencilStoreOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

				//	subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				//	subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				//	subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				//	subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				//	ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				//	subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
				//	break;
				//}
				//case ATTACHMENT_TYPE::DEPTH_STENCIL:
				//{
				//	attDesc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				//	attDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				//	// TODO - Should this be considered a stencil or regular load/store op?
				//	attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
				//	attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

				//	subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; // Store op is always performed in late tests, after subpass access
				//	subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; // Load op is always performed in early tests, before subpass access
				//	subpassDesc.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				//	subpassDesc.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				//	ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
				//	subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
				//	break;
				//}
				//case ATTACHMENT_TYPE::RESOLVE:
				//{
				//	attDesc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				//	attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				//	attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
				//	attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

				//	subpassDesc.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				//	subpassDesc.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // TODO - optimize
				//	subpassDesc.srcAccessMask |= VK_ACCESS_NONE;
				//	subpassDesc.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

				//	ASSERT_MSG(subpassDesc.resolveAttachmentIndex == -1, "Already assigned the resolve attachment index!");
				//	subpassDesc.resolveAttachmentIndex = localResourceIndex;
				//	break;
				//}
				//}
					
				ASSERT_ALWAYS("Failed to find output barrier for render pass?");
			}
			else
			{
				const Barrier& outputBarrier = iter->second;

				attDesc.initialLayout = pTexture->GetLayout();
				attDesc.layout = outputBarrier.oldLayout;
				attDesc.finalLayout = outputBarrier.newLayout;

				subpassDesc.srcAccessMask |= outputBarrier.srcAccessMask;
				subpassDesc.dstAccessMask |= outputBarrier.dstAccessMask;
				subpassDesc.srcStageMask |= outputBarrier.srcStageMask;
				subpassDesc.dstStageMask |= outputBarrier.dstStageMask;

				switch (resourceUsage->attachmentType)
				{
				case ATTACHMENT_TYPE::COLOR:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

					subpassDesc.colorAttachmentIndices.push_back(localResourceIndex);
					break;
				}
				case ATTACHMENT_TYPE::DEPTH:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::STENCIL:
				{
					attDesc.stencilLoadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
					attDesc.stencilStoreOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::DEPTH_STENCIL:
				{
					// TODO - Should this be considered a stencil or regular load/store op?
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::RESOLVE:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(resourceUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(resourceUsage->storeOp);

					ASSERT_MSG(subpassDesc.resolveAttachmentIndex == -1, "Already assigned the resolve attachment index!");
					subpassDesc.resolveAttachmentIndex = localResourceIndex;
					break;
				}
				}
			}

			renderPassDesc.attachments.push_back(attDesc);

			localResourceIndex++;
		});

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
		TraverseRenderPassOutputs(renderPass.m_index, [&](const RenderResource& outputResource)
		{
			if (outputResource.type != RESOURCE_TYPE::TEXTURE)
			{
				return;
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
				return;
			}

			const ResourceUsage* resourceUsage = GetResourceUsageFromPass(renderPass, outputResource.resourceID);
			if (resourceUsage == nullptr)
			{
				ASSERT_ALWAYS("Failed to create framebuffer. Render pass uses physical resource but has no usage for it?");
				return;
			}

			FramebufferAttachmentDesc desc;
			desc.pTexture = static_cast<TextureVk*>(outputResource.data);
			desc.mipTarget = 0;
			desc.type = resourceUsage->attachmentType;
			desc.storeOp = resourceUsage->storeOp;
			desc.loadOp = resourceUsage->loadOp;

			attachments.push_back(desc);

			maxWidth = Max(maxWidth, pAttachmentTex->GetWidth());
			maxHeight = Max(maxHeight, pAttachmentTex->GetHeight());
		}
		);

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

	u8 RenderGraphVk::RegisterResource(void* data, RESOURCE_TYPE type, const ResourceUsage& usage)
	{
		const u64 resourceID = HashResource(data, type);
		u8 physicalResourceIndex = U8_MAX;

		// Try to find an existing physical resource
		for (u8 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			const RenderResource& physicalResource = m_physicalResources[i];
			if (physicalResource.resourceID == resourceID)
			{
				// Found match
				physicalResourceIndex = i;
				break;
			}
		}
		
		if (physicalResourceIndex == U8_MAX)
		{
			// Couldn't find existing physical resource, create one instead
			RenderResource newPhysicalResource{};
			newPhysicalResource.data = data;
			newPhysicalResource.resourceID = resourceID;
			newPhysicalResource.type = type;

			physicalResourceIndex = static_cast<u8>(m_physicalResources.size());
			m_physicalResources.push_back(newPhysicalResource);
		}

		// Always create new virtual resource
		ResourceUsage newUsage = usage;
		newUsage.resourceID = resourceID;
		m_resourceUsages.push_back(newUsage);

		return physicalResourceIndex;
	}

	DeviceContextVk* RenderGraphVk::GetDeviceContext() const
	{
		return m_deviceContexts[m_frameIndex];
	}

	u32 RenderGraphVk::FindBackBufferRenderPassIndex()
	{
		u32 backbufferRPIndex = s_invalidRenderPassIndex;

		for (u32 j = 0; j < m_resourceUsages.size(); j++)
		{
			const ResourceUsage& resourceUsage = m_resourceUsages[j];
			if (resourceUsage.io != RESOURCE_IO::OUTPUT)
			{
				// Only consider output resources
				continue;
			}

			if (resourceUsage.name == nullptr)
			{
				continue;
			}

			// TODO - We might want to store the CRC rather than the raw string
			const CRC32 outputResourceCRC = HashCRC32(resourceUsage.name);
			if (outputResourceCRC == m_reservedBackbufferNameCRC)
			{
				backbufferRPIndex = resourceUsage.passIndex;
				break;
			}
		}

		return backbufferRPIndex;
	}

	void RenderGraphVk::BuildDependencyTree(u32 renderPassIndex)
	{
		RenderPassVk& currRenderPass = m_registeredRenderPasses[renderPassIndex];

		// Base cases
		if (currRenderPass.m_inputResources.none())
		{
			return;
		}
		if (renderPassIndex >= m_registeredRenderPasses.size())
		{
			return;
		}

		// Recursively find all other passes EARLIER IN SUBMISSION ORDER which pose an access 
		// hazard to any of the resources in the current pass. These hazards can be read-after-write (RAW),
		// write-after-read (WAR) and write-after-write (WAW)
		for (u32 i = 0; i < renderPassIndex; i++)
		{
			// Prevent a render pass from listing itself as a dependency
			if (renderPassIndex == i)
			{
				continue;
			}

			RenderPassVk& prevRenderPass = m_registeredRenderPasses[i];

			const ResourceIndexBitset rawHazardResources = (prevRenderPass.m_outputResources & currRenderPass.m_inputResources);
			const ResourceIndexBitset warHazardResources = (prevRenderPass.m_inputResources & currRenderPass.m_outputResources);
			const ResourceIndexBitset wawHazardResources = (prevRenderPass.m_outputResources & currRenderPass.m_outputResources);

			// Create a new dependency to this previous render pass if any hazards are detected
			const ResourceIndexBitset hazardResources = (rawHazardResources | warHazardResources | wawHazardResources);
			const bool isDependencyRP = hazardResources.any();
			if (isDependencyRP)
			{
				DependencyInfo newDependency{};
				newDependency.renderPass = &prevRenderPass;
				newDependency.resources = hazardResources;
				currRenderPass.m_dependencyInfos.push_back(newDependency);
				BuildDependencyTree(i);
			}
		}
	}

	void RenderGraphVk::FindActivePasses(u32 finalPassIndex, std::vector<u32>& out_activeRenderPasses)
	{
		// Traverse dependency tree and tag all passes which contribute to the final pass
		// Discard any passes which weren't tagged
		const u32 passCount = static_cast<u32>(m_registeredRenderPasses.size());
		std::vector<bool> contributesToFinalPass(passCount, false);

		TraverseDependencyTree(finalPassIndex, [&](const RenderPassVk& currRenderPass)
		{
			out_activeRenderPasses.push_back(currRenderPass.m_index);
		});
	}

	void RenderGraphVk::CalculateResourceBarriers(const std::vector<u32>& activeRenderPasses)
	{
		// Traverse the dependency tree from bottom-to-top, and for every dependency:
		// 1. Find which resource usages caused that dependency
		// 2. For all those resource usages, generate a barrier. Only generate pipeline barriers for now, and ignore cross-queue synchronization
		for (u32 activeRenderPassIndex : activeRenderPasses)
		{
			RenderPassVk& dstRenderPass = m_registeredRenderPasses[activeRenderPassIndex];

			// Special case for first and last passes in submission order, since dependencies won't fill out
			// their input/output dependencies respectively
			const bool isFirst = (activeRenderPassIndex == activeRenderPasses.back());
			const bool isLast = (activeRenderPassIndex == activeRenderPasses[0]);
			if (isLast)
			{
				TraverseRenderPassOutputs(dstRenderPass.m_index, [&](const RenderResource& resource)
				{
					if (resource.type != RESOURCE_TYPE::TEXTURE)
					{
						return;
					}

					const u64& resourceID = resource.resourceID;
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(dstRenderPass, resourceID);
					ASSERT_PTR(dstResourceUsage); // Should never be null

					// Special cases for backbuffer (presentation engine) and depth buffer
					CRC32 dstResourceCRC = HashCRC32(dstResourceUsage->name);
					const bool isBackBufferResource = (dstResourceCRC == m_reservedBackbufferNameCRC);
					const bool isDepthBufferResource = (dstResourceCRC == m_reservedDepthBufferNameCRC);

					TextureVk* pTexture = static_cast<TextureVk*>(resource.data);
					ASSERT_PTR(pTexture);

					const VkImageLayout layout = CalculateResourceImageLayout(*dstResourceUsage, dstRenderPass.m_bindPoint);
					const BIND_POINT dstBindPoint = dstRenderPass.m_bindPoint;

					Barrier newDstBarrier;
					if (isBackBufferResource)
					{
						newDstBarrier.oldLayout = layout;
						newDstBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

						// NOTE - Presentation engine is external and doesn't require an access mask, but if the backbuffer
						//        is cleared (from loadOp) we must still sync the clear operation. As a result, we'll set
						//        the dst flags to COLOR_ATTACHMENT-related write operations to be safe
						newDstBarrier.srcAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
						newDstBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						newDstBarrier.srcStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.srcAccessMask, true);
						newDstBarrier.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					}
					else if (isDepthBufferResource)
					{
						newDstBarrier.oldLayout = layout;
						newDstBarrier.newLayout = layout;

						newDstBarrier.srcAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
						newDstBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						newDstBarrier.srcStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.srcAccessMask, true);
						newDstBarrier.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
					}

					dstRenderPass.m_outputBarriers.insert({ resourceID, newDstBarrier });
				});
			}
			else if (isFirst)
			{
				// Transition all resources used in the first pass to the correct layout, from whatever they were in the previous frame
				const ResourceIndexBitset IOResources = (dstRenderPass.m_inputResources | dstRenderPass.m_outputResources);
				TraverseResources(IOResources, [&](const RenderResource& resource)
				{
					if (resource.type != RESOURCE_TYPE::TEXTURE)
					{
						return;
					}
					TextureVk* pTexture = static_cast<TextureVk*>(resource.data);
					ASSERT_PTR(pTexture);

					const u64& resourceID = resource.resourceID;
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(dstRenderPass, resourceID);
					ASSERT_PTR(dstResourceUsage); // Should never be null

					// Only insert barriers if the layout is not compatible with the render passes' usage
					const VkImageLayout srcLayout = pTexture->GetLayout();
					const VkImageLayout dstLayout = CalculateResourceImageLayout(*dstResourceUsage, dstRenderPass.m_bindPoint);
					if (srcLayout != dstLayout)
					{
						const BIND_POINT dstBindPoint = dstRenderPass.m_bindPoint;

						Barrier newDstBarrier;
						newDstBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
						newDstBarrier.dstAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
						newDstBarrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
						newDstBarrier.dstStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.dstAccessMask, false);
						newDstBarrier.oldLayout = srcLayout;
						newDstBarrier.newLayout = dstLayout;

						dstRenderPass.m_inputBarriers.insert({ resourceID, newDstBarrier });
					}
				});
			}

			for (const DependencyInfo& dependencyInfo : dstRenderPass.m_dependencyInfos)
			{
				RenderPassVk* srcRenderPass = dependencyInfo.renderPass;
				TraverseResources(dependencyInfo.resources, [&](const RenderResource& resourceDependency)
				{
					// Setup the barrier. In this case the source corresponds to the active source render pass we're
					// currently in. The destination corresponds to the dependency we're currently looping through
					const u64& resourceID = resourceDependency.resourceID;
					const ResourceUsage* srcResourceUsage = GetResourceUsageFromPass(*srcRenderPass, resourceID);
					ASSERT_PTR(srcResourceUsage); // Should never be null
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(dstRenderPass, resourceID);
					ASSERT_PTR(dstResourceUsage); // Should never be null

					const BIND_POINT srcBindPoint = srcRenderPass->m_bindPoint;
					const BIND_POINT dstBindPoint = dstRenderPass.m_bindPoint;

					Barrier newDstBarrier;
					newDstBarrier.srcAccessMask = CalculateResourceAccessFlags(*srcResourceUsage, resourceDependency, srcBindPoint);
					newDstBarrier.dstAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resourceDependency, dstBindPoint);
					newDstBarrier.srcStageMask = CalculateResourcePipelineStageFlags(srcBindPoint, newDstBarrier.srcAccessMask, true);
					newDstBarrier.dstStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.dstAccessMask, false);

					if (resourceDependency.type == RESOURCE_TYPE::TEXTURE)
					{
						newDstBarrier.oldLayout = CalculateResourceImageLayout(*srcResourceUsage, srcBindPoint);
						newDstBarrier.newLayout = CalculateResourceImageLayout(*dstResourceUsage, dstBindPoint);
					}

					// Add the barrier information to both src and dst pass
					dstRenderPass.m_inputBarriers.insert({ resourceID, newDstBarrier });
					srcRenderPass->m_outputBarriers.insert({ resourceID, newDstBarrier });
				});
			}
		}
	}

	STATUS_CODE RenderGraphVk::InsertResourceBarriers(const RenderPassVk& renderPass)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;
		DeviceContextVk* pDeviceContext = GetDeviceContext();

		for (auto& barrierIter : renderPass.m_inputBarriers)
		{
			u64 resourceID = barrierIter.first;
			const Barrier& currBarrier = barrierIter.second;

			const RenderResource* resourceBarrier = GetPhysicalResource(resourceID);
			ASSERT_PTR(resourceBarrier);

			switch (resourceBarrier->type)
			{
			case RESOURCE_TYPE::BUFFER:
			{
				BufferVk* pBuffer = static_cast<BufferVk*>(resourceBarrier->data);
				res = pDeviceContext->InsertBufferMemoryBarrier(
					pBuffer,
					ConvertBindPointToQueueType(renderPass.m_bindPoint),
					currBarrier.srcStageMask,
					currBarrier.dstStageMask,
					currBarrier.srcAccessMask,
					currBarrier.dstAccessMask
				);

				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to insert dependency barriers. Could not insert buffer memory barrier!");
					return res;
				}

				break;
			}
			case RESOURCE_TYPE::TEXTURE:
			{
				TextureVk* pTexture = static_cast<TextureVk*>(resourceBarrier->data);
				res = pDeviceContext->InsertImageMemoryBarrier(
					pTexture,
					ConvertBindPointToQueueType(renderPass.m_bindPoint),
					currBarrier.srcStageMask,
					currBarrier.dstStageMask,
					currBarrier.srcAccessMask,
					currBarrier.dstAccessMask,
					currBarrier.oldLayout,
					currBarrier.newLayout
				);

				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to insert dependency barriers. Could not insert image memory barrier!");
					return res;
				}

				// Update the texture's internal layout variable so it matches it's actual layout
				pTexture->SetLayout(currBarrier.newLayout);

				break;
			}
			case RESOURCE_TYPE::UNIFORM:
			{
				// Is this valid?
				TODO();
				break;
			}
			}
		}

		return res;
	}

	void RenderGraphVk::TraverseDependencyTree(u32 renderPassIndex, TraverseDependenciesCallbackFn callback)
	{
		// Depth-first traversal
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.size()))
		{
			return;
		}

		const RenderPassVk& currRenderPass = m_registeredRenderPasses[renderPassIndex];
		if (callback != nullptr)
		{
			callback(currRenderPass);
		}

		for (u32 i = 0; i < static_cast<u32>(currRenderPass.m_dependencyInfos.size()); i++)
		{
			const DependencyInfo& dependencyInfo = currRenderPass.m_dependencyInfos[i];
			ASSERT_PTR(dependencyInfo.renderPass);
			TraverseDependencyTree(dependencyInfo.renderPass->m_index, callback);
		}
	}

	void RenderGraphVk::TraverseResources(const ResourceIndexBitset& resourceBitset, TraverseResourceCallbackFn callback) const
	{
		for (u32 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			if (resourceBitset.test(i))
			{
				const RenderResource& resource = m_physicalResources[i];
				callback(resource);
			}
		}
	}

	void RenderGraphVk::TraverseRenderPassInputs(u32 renderPassIndex, TraverseResourceCallbackFn callback) const
	{
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.size()))
		{
			return;
		}

		const RenderPassVk& rp = m_registeredRenderPasses[renderPassIndex];
		TraverseResources(rp.m_inputResources, callback);
	}

	void RenderGraphVk::TraverseRenderPassOutputs(u32 renderPassIndex, TraverseResourceCallbackFn callback) const
	{
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.size()))
		{
			return;
		}

		const RenderPassVk& rp = m_registeredRenderPasses[renderPassIndex];
		TraverseResources(rp.m_outputResources, callback);
	}

	const ResourceUsage* RenderGraphVk::GetResourceUsageFromPass(const RenderPassVk& renderPass, u64 resourceID) const
	{
		u32 passIndex = renderPass.m_index;
		for (u32 i = 0; i < static_cast<u32>(m_resourceUsages.size()); i++)
		{
			const ResourceUsage& usage = m_resourceUsages[i];
			if ((passIndex == usage.passIndex) && (usage.resourceID == resourceID))
			{
				return &usage;
			}
		}

		return nullptr;
	}

	const RenderResource* RenderGraphVk::GetPhysicalResource(u64 resourceID) const
	{
		// TODO - Replace lookup with a map
		for (u32 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			const RenderResource& resource = m_physicalResources[i];
			if (resource.resourceID == resourceID)
			{
				return &resource;
			}
		}

		return nullptr;
	}

	void RenderGraphVk::UpdateTextureLayouts(u32 renderPassIndex)
	{
		RenderPassVk& renderPass = m_registeredRenderPasses[renderPassIndex];
		for (const auto& iter : renderPass.m_outputBarriers)
		{
			u64 resourceID = iter.first;
			const Barrier& barrierInfo = iter.second;

			const RenderResource* resource = GetPhysicalResource(resourceID);
			ASSERT_PTR(resource);

			TextureVk* textureResource = static_cast<TextureVk*>(resource->data);
			ASSERT_PTR(textureResource);

			textureResource->SetLayout(barrierInfo.newLayout);
		}
	}
}