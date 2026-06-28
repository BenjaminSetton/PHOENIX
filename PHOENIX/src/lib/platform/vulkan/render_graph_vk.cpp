
#include <sstream>
#include <vulkan/vk_enum_string_helper.h>

#include "render_graph_vk.h"

#include "buffer_vk.h"
#include "core/handle/handle_accessor.h"
#include "core/handle/handle_utils.h"
#include "device_context_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "texture_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/cache_utils.h"
#include "utils/file_io.h"
#include "utils/logger.h"
#include "utils/math.h"
#include "utils/render_graph_type_converter.h"
#include "utils/sanity.h"

// Render graph inspired from:
// https://poniesandlight.co.uk/reflect/island_rendergraph_1/
//

namespace PHX
{
	static const char* s_pReservedBackbufferName = "INTERNAL_backbuffer";
	static const char* s_pReservedDepthBufferName = "INTERNAL_depthbuffer";
	static constexpr u32 s_invalidRenderPassIndex = U32_MAX;

	static u64 HashResource(Handle resource, const RESOURCE_TYPE& type)
	{
		size_t seed = 0;
		HashCombine(seed, HandleAccessor::GetIndex(resource));
		HashCombine(seed, HandleAccessor::GetGeneration(resource));
		HashCombine(seed, type);

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

	static ATTACHMENT_TYPE CalculateAttachmentType(TextureHandle handle)
	{
		AspectTypeFlags aspectFlags = handle.GetAspectFlags();
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
					const BUFFER_USAGE bufferUsage = usage.bufferUsage;
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
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);
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
				break;
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

	void RenderPassVk::SetTextureInput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;
		usage.attachmentType = CalculateAttachmentType(texture);
		usage.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::LOAD;

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferInput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;
		usage.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER; // Not sure if this is correct

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetUniformInput(UniformCollectionHandle uniformCollection)
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

		const u8 resourceIndex = m_registerResourceCallback(uniformCollection, RESOURCE_TYPE::UNIFORM, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetColorOutput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::COLOR;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthOutput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.name = s_pReservedDepthBufferName; // HACK - Assuming all depth writes are for the depth buffer
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthStencilOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH_STENCIL;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetResolveOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.name = nullptr; // TODO
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::RESOLVE;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBackbufferOutput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.name = s_pReservedBackbufferName;
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::COLOR;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		// TODO - Mark this resource as backbuffer

		const u8 resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferOutput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.name = nullptr;
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.bufferUsage = BUFFER_USAGE::UNIFORM_BUFFER; // Not sure if this is correct

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const u8 resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
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

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice) : m_pRenderDevice(nullptr), m_deviceContextHandles(), 
		m_frameInFlightIndex(0), m_frameNumber(0), m_reservedBackbufferNameCRC(HashCRC32(s_pReservedBackbufferName)),
		m_reservedDepthBufferNameCRC(HashCRC32(s_pReservedDepthBufferName))
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to initialize render graph. Render device is null!");
			return;
		}

		m_pRenderDevice = pRenderDevice;

		const u32 framesInFlight = m_pRenderDevice->GetFramesInFlight();
		for (u32 i = 0; i < framesInFlight; i++)
		{
			DeviceContextHandle deviceContext;
			STATUS_CODE res = m_pRenderDevice->AllocateDeviceContext({}, deviceContext);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to construct render graph. Device context creation failed!");
				return;
			}
			m_deviceContextHandles.push_back(deviceContext);
		}
	}

	RenderGraphVk::~RenderGraphVk()
	{
	}

	STATUS_CODE RenderGraphVk::BeginFrame(SwapChainHandle swapChain)
	{
		if (!swapChain.IsValid())
		{
			LogError("Failed to begin frame. Swap chain handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(m_pRenderDevice->ResolveHandle(swapChain));
		ASSERT_PTR(swapChainVk);

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetDeviceContext());
		res = pDeviceContext->BeginFrame(swapChainVk, m_frameInFlightIndex);
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

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetDeviceContext());
		res = pDeviceContext->EndFrame(m_frameInFlightIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame. Device context could not flush!");
		}

		// Now that all the work has been done for the current frame, move onto the next one
		m_frameInFlightIndex = (m_frameInFlightIndex + 1) % m_pRenderDevice->GetFramesInFlight();
		m_frameNumber++;

		m_registeredRenderPasses.clear();
		m_resourceUsages.clear();
		m_physicalResources.clear();

		return res;
	}

	STATUS_CODE RenderGraphVk::RegisterPass(const char* passName, BIND_POINT bindPoint, RenderPassHandle& renderPass)
	{
		const u32 passIndex = static_cast<u32>(m_registeredRenderPasses.size());

		// TODO - Reconcile with HANDLE_UTILS functions
		auto registerResourceFuncPtr = std::bind(&RenderGraphVk::RegisterResource, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		m_registeredRenderPasses.emplace_back(passName, bindPoint, passIndex, registerResourceFuncPtr);

		RenderPassVk& passVk = m_registeredRenderPasses.back();
		passVk.IncrementRefCount();
		HandleAccessor::PopulateHandle(renderPass, this, static_cast<u32>(m_registeredRenderPasses.size() - 1), 0u);
		return STATUS_CODE::SUCCESS;
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

		CalculateResourceBarriers(activeRenderPassIndices, finalRPIndex);

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO
		
		// Now that the render graph has been generated, run through it and perform the following steps for each render pass:
		// 1. Declare the resources that will get used in the device context
		// 2. Insert resource barriers and/or perform layout transitions as necessary
		// 3. Call the execute callback and pass in the device context
		std::reverse(activeRenderPassIndices.begin(), activeRenderPassIndices.end());

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetDeviceContext());
		DeviceContextHandle deviceContext = GetDeviceContextHandle();

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
					// Get or create render pass (refers to internal cache)
					VkRenderPass renderPassVk = CreateRenderPass(currRenderPass);
		
					// Get or create framebuffer from render device (refers to internal cache)
					const bool isBackbuffer = (currRenderPass.m_index == finalRPIndex);
					FramebufferVk* pFramebuffer = CreateFramebuffer(currRenderPass, renderPassVk, isBackbuffer);

					// Get or create pipeline from render device (refers to internal cache)
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, renderPassVk);

					res = pDeviceContext->BeginRenderPass(renderPassVk, pFramebuffer, pClearColors, clearColorCount);
					if (res != STATUS_CODE::SUCCESS)
					{
						LogError("Failed to bake render pass. Device context could not begin render pass!");
						return res;
					}

					// Call the main render pass execution callback with a device context handle, since it's client-facing
					pDeviceContext->SetContextualPipeline(pPipeline);
					currRenderPass.m_execCallback(deviceContext);
					pDeviceContext->ResetContextualPipeline();

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
					// Get or create pipeline from render device (refes to internal cache)
					// NOTE - The render pass isn't used for compute pipeline creation, so it can
					// be ignored by passing in VK_NULL_HANDLE
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, VK_NULL_HANDLE);

					pDeviceContext->SetContextualPipeline(pPipeline);
					currRenderPass.m_execCallback(deviceContext);
					pDeviceContext->ResetContextualPipeline();

					break;
				}
				case BIND_POINT::TRANSFER:
				{
					// Transfer-only passes do not use a pipeline
					currRenderPass.m_execCallback(deviceContext);

					break;
				}
			}
		}

		return res;
	}

	u32 RenderGraphVk::GetFrameNumber() const
	{
		return m_frameNumber;
	}

	STATUS_CODE RenderGraphVk::GenerateVisualization(const char* fileName, bool generateIfUnique)
	{
		UNUSED(generateIfUnique);
		if (generateIfUnique)
		{
			TODO();
		}

		if (fileName == nullptr)
		{
			LogError("Failed to generate render graph visualization. File name is null!");
			return STATUS_CODE::ERR_API;
		}

		if (m_registeredRenderPasses.empty())
		{
			LogError("Failed to generate render graph visualization. No render passes registered - call after Bake()!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		const u32 backbufferPassIndex = FindBackBufferRenderPassIndex();

		// Looks up a usage name for a physical resource, used to detect the reserved backbuffer/depth resources
		auto getResourceUsageName = [&](u64 resourceID) -> const char*
		{
			for (const ResourceUsage& usage : m_resourceUsages)
			{
				if (usage.resourceID == resourceID && usage.name != nullptr)
				{
					return usage.name;
				}
			}
			return nullptr;
		};

		// Builds a verbose barrier tooltip (stage + access masks) shown on hover in SVG output
		auto buildBarrierTooltip = [&](const Barrier& barrier) -> std::string
		{
			std::ostringstream tip;
			tip << "stage: " << string_VkPipelineStageFlags(barrier.srcStageMask) << " -> " << string_VkPipelineStageFlags(barrier.dstStageMask)
				<< " | access: " << string_VkAccessFlags(barrier.srcAccessMask) << " -> " << string_VkAccessFlags(barrier.dstAccessMask);
			return tip.str();
		};

		std::ostringstream dot;
		dot << "digraph RenderGraph {\n";
		dot << "\trankdir=LR;\n";
		dot << "\tbgcolor=\"#FBFCFC\";\n";
		dot << "\tnodesep=0.35;\n";
		dot << "\tranksep=1.0;\n";
		dot << "\tnode [fontname=\"Helvetica\", fontsize=11];\n";
		dot << "\tedge [fontname=\"Helvetica\", fontsize=9, arrowsize=0.8];\n\n";

		// ---- Render pass nodes (rounded boxes, colored by bind point) ----
		dot << "\t// Render passes\n";
		for (const RenderPassVk& pass : m_registeredRenderPasses)
		{
#if defined(PHX_DEBUG)
			const char* passName = pass.m_debugName;
#else
			const std::string passNameStr = std::to_string(pass.m_index);
			const char* passName = passNameStr.c_str();
#endif
			const char* bindPointStr = RG_UTILS::BindPointToString(pass.m_bindPoint);

			const char* fillColor = "#FFFFFF";
			if (pass.m_bindPoint == BIND_POINT::GRAPHICS)       fillColor = "#5DADE2";
			else if (pass.m_bindPoint == BIND_POINT::COMPUTE)   fillColor = "#58D68D";
			else if (pass.m_bindPoint == BIND_POINT::TRANSFER)  fillColor = "#EB984E";

			const bool isFinalPass = (pass.m_index == backbufferPassIndex);

			dot << "\tpass" << pass.m_index
				<< " [shape=box, style=\"filled,rounded\", fontcolor=\"#FFFFFF\", margin=\"0.25,0.14\""
				<< ", fillcolor=\"" << fillColor << "\"";
			if (isFinalPass)  dot << ", penwidth=3, color=\"#C0392B\"";
			else              dot << ", penwidth=1, color=\"#34495E\"";

			dot << ", label=<<b>" << passName << "</b><br/><font point-size=\"9\">" << bindPointStr;
			if (isFinalPass)  dot << " &#8226; FINAL";
			dot << "</font>>];\n";
		}

		dot << "\n";

		// ---- Resource nodes ----
		// Gather every physical resource referenced by any pass (inputs or outputs)
		ResourceIndexBitset usedResources;
		for (const RenderPassVk& pass : m_registeredRenderPasses)
		{
			usedResources |= pass.m_inputResources;
			usedResources |= pass.m_outputResources;
		}

		dot << "\t// Resources\n";
		TraverseResources(usedResources, [&](const RenderResource& resource)
		{
			const std::string nodeId = "res" + std::to_string(resource.resourceID);

			const char* usageName = getResourceUsageName(resource.resourceID);
			const bool isBackbuffer = (usageName != nullptr) && (HashCRC32(usageName) == m_reservedBackbufferNameCRC);
			const bool isDepth      = (usageName != nullptr) && (HashCRC32(usageName) == m_reservedDepthBufferNameCRC);

			// Defaults (texture). All resources share a single ellipse shape and are
			// distinguished from passes (rounded boxes) by shape, and from each other by color.
			std::string displayName = "Texture";
			const char* typeTag = "TEXTURE";
			const char* fill    = "#EBF5FB";
			const char* border  = "#2E86C1";
			u32 penWidth        = 1;

			if (resource.type == RESOURCE_TYPE::BUFFER)
			{
				displayName = GetResourceName(resource);
				typeTag = "BUFFER";
				fill    = "#F4ECF7";
				border  = "#8E44AD";
			}
			else if (resource.type == RESOURCE_TYPE::UNIFORM)
			{
				displayName = "Uniforms";
				typeTag = "UNIFORM";
				fill    = "#FEF9E7";
				border  = "#B7950B";
			}
			else // TEXTURE
			{
				displayName = GetResourceName(resource);
			}

			if (isBackbuffer)
			{
				displayName = "Backbuffer";
				typeTag = "PRESENT";
				fill    = "#FADBD8";
				border  = "#C0392B";
				penWidth = 3;
			}
			else if (isDepth)
			{
				typeTag = "DEPTH";
				fill    = "#FCF3CF";
				border  = "#B7950B";
			}

			dot << "\t" << nodeId << " [shape=ellipse, style=filled, fillcolor=\"" << fill
				<< "\", color=\"" << border << "\", penwidth=" << penWidth
				<< ", label=<<b>" << displayName << "</b><br/><font point-size=\"8\" color=\"#5D6D7E\">" << typeTag << "</font>>];\n";
		});

		dot << "\n\t// Resource flow (inputs feed passes, passes produce outputs)\n";

		// ---- Edges: resource -> pass (inputs) and pass -> resource (outputs) ----
		for (const RenderPassVk& pass : m_registeredRenderPasses)
		{
			const std::string passNode = "pass" + std::to_string(pass.m_index);

			// Inputs: resource -> pass (blue), labelled with the input layout transition for textures
			TraverseResources(pass.m_inputResources, [&](const RenderResource& resource)
			{
				const std::string resNode = "res" + std::to_string(resource.resourceID);

				std::string label;
				std::string tooltip;
				auto barrierIter = pass.m_inputBarriers.find(resource.resourceID);
				if (barrierIter != pass.m_inputBarriers.end())
				{
					const Barrier& barrier = barrierIter->second;
					if (resource.type == RESOURCE_TYPE::TEXTURE)
					{
						label = RG_UTILS::ShortImageLayout(barrier.oldLayout) + "\\n-> " + RG_UTILS::ShortImageLayout(barrier.newLayout);
					}
					tooltip = buildBarrierTooltip(barrier);
				}

				dot << "\t" << resNode << " -> " << passNode << " [color=\"#2E86C1\"";
				if (!label.empty())   dot << ", label=\"" << label << "\", fontcolor=\"#1F618D\"";
				if (!tooltip.empty()) dot << ", labeltooltip=\"" << tooltip << "\", edgetooltip=\"" << tooltip << "\"";
				dot << "];\n";
			});

			// Outputs: pass -> resource (green), labelled with the resulting layout for textures
			TraverseResources(pass.m_outputResources, [&](const RenderResource& resource)
			{
				const std::string resNode = "res" + std::to_string(resource.resourceID);

				std::string label;
				std::string tooltip;
				auto barrierIter = pass.m_outputBarriers.find(resource.resourceID);
				if (barrierIter != pass.m_outputBarriers.end())
				{
					const Barrier& barrier = barrierIter->second;
					if (resource.type == RESOURCE_TYPE::TEXTURE)
					{
						label = "-> " + RG_UTILS::ShortImageLayout(barrier.newLayout);
					}
					tooltip = buildBarrierTooltip(barrier);
				}

				dot << "\t" << passNode << " -> " << resNode << " [color=\"#239B56\", penwidth=1.4";
				if (!label.empty())   dot << ", label=\"" << label << "\", fontcolor=\"#1E8449\"";
				if (!tooltip.empty()) dot << ", labeltooltip=\"" << tooltip << "\", edgetooltip=\"" << tooltip << "\"";
				dot << "];\n";
			});
		}

		// ---- Legend ----
		dot << "\n\t// Legend (floating, not connected to the graph)\n";
		dot << "\tlegend [shape=box, style=filled, fillcolor=\"#FFFFFF\", color=\"#34495E\", margin=0, label=<\n";
		dot << "\t\t<TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"4\" CELLPADDING=\"3\">\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"9\" COLOR=\"#5D6D7E\">Passes = rounded boxes &#8226; Resources = ellipses</FONT></TD></TR>\n";

		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"10\"><B>Passes</B></FONT></TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#5DADE2\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Graphics pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#58D68D\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Compute pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#EB984E\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Transfer pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#FFFFFF\" BORDER=\"3\" COLOR=\"#C0392B\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Final (backbuffer) pass</TD></TR>\n";

		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"10\"><B>Resources</B></FONT></TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#EBF5FB\" BORDER=\"1\" COLOR=\"#2E86C1\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Texture</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#F4ECF7\" BORDER=\"1\" COLOR=\"#8E44AD\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Buffer</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#FEF9E7\" BORDER=\"1\" COLOR=\"#B7950B\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Uniform</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#FCF3CF\" BORDER=\"1\" COLOR=\"#B7950B\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Depth buffer</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#FADBD8\" BORDER=\"3\" COLOR=\"#C0392B\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Backbuffer (present)</TD></TR>\n";

		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"10\"><B>Edges</B></FONT></TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#2E86C1\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Resource &#8594; Pass (read / input)</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#239B56\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Pass &#8594; Resource (write / output)</TD></TR>\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"9\" COLOR=\"#5D6D7E\">Edge labels show texture layout transitions</FONT></TD></TR>\n";
		dot << "\t\t</TABLE>>];\n";

		dot << "}\n";


		// Write to file
		{
			FileIO io(fileName);
			if (!io.IsOpen())
			{
				LogError("Failed to generate render graph visualization. Could not open file \"%s\" for writing!", fileName);
				return STATUS_CODE::ERR_INTERNAL;
			}

			const std::string dotStr = dot.str();
			io.Write(dotStr.c_str(), static_cast<u32>(dotStr.size()));
		}

		LogDebug("Render graph visualization written to \"%s\"", fileName);
		return STATUS_CODE::SUCCESS;
	}

	IDeviceContext* RenderGraphVk::GetDeviceContext()
	{
		if (m_frameInFlightIndex >= m_deviceContextHandles.size())
		{
			// This should never happen!
			ASSERT_ALWAYS("Failed to get device context at index %u. Index is out of bounds!");
			return nullptr;
		}

		DeviceContextHandle deviceContext = m_deviceContextHandles[m_frameInFlightIndex];
		return HANDLE_UTILS::ResolveHandle(deviceContext);
	}

	DeviceContextHandle RenderGraphVk::GetDeviceContextHandle()
	{
		ASSERT(m_frameInFlightIndex < m_deviceContextHandles.size());
		return m_deviceContextHandles[m_frameInFlightIndex];
	}

	void* RenderGraphVk::ResolveHandle(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS:
		{
			// TODO - Reconcile with HANDLE_UTILS functions
			const u32 index = HandleAccessor::GetIndex(handle);
			if (index < static_cast<u32>(m_registeredRenderPasses.size()))
			{
				return &m_registeredRenderPasses[index];
			}
			break;
		}
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to resolve handle. Unsupported handle type!");
		return nullptr;
	}

	void RenderGraphVk::IncrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS:
		{
			HANDLE_UTILS::IncrementRefCount<RenderPassHandle, IRenderPass>(handle);
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to resolve handle. Unsupported handle type!");
			break;
		}
		}
	}

	void RenderGraphVk::DecrementHandleRefCount(const Handle& handle)
	{
		const HANDLE_TYPE type = HandleAccessor::GetType(handle);
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS:
		{
			// TODO - Reconcile with HANDLE_UTILS functions
			const u32 index = HandleAccessor::GetIndex(handle);
			if (index < static_cast<u32>(m_registeredRenderPasses.size()))
			{
				RenderPassVk renderPass = m_registeredRenderPasses[index];
				renderPass.DecrementRefCount();

				// NOTE - No automatic deletion, render pass lifetimes are managed by other render graph calls
			}
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to resolve handle. Unsupported handle type!");
			break;
		}
		}
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

			TextureVk* pTexture = ResolveTexture(outputResource);
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

		VkRenderPass renderPassVk = m_pRenderDevice->CreateRenderPass(renderPassDesc);
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

			TextureVk* pAttachmentTex = ResolveTexture(outputResource);
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
			desc.pTexture = ResolveTexture(outputResource);
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
		framebufferCI.width = maxWidth; // TODO - Revisit
		framebufferCI.height = maxHeight; // TODO - Revisit
		framebufferCI.layers = 1;
		framebufferCI.pAttachments = attachments.data();
		framebufferCI.attachmentCount = static_cast<u32>(attachments.size());
		framebufferCI.renderPass = renderPassVk;
		framebufferCI.isBackbuffer = isBackBuffer;
		FramebufferVk* pFramebuffer = m_pRenderDevice->CreateFramebuffer(framebufferCI);
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
			pipeline = m_pRenderDevice->CreateGraphicsPipeline(renderPass.graphicsDesc, renderPassVk);
			break;
		}
		case BIND_POINT::COMPUTE:
		{
			pipeline = m_pRenderDevice->CreateComputePipeline(renderPass.computeDesc);
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

	u8 RenderGraphVk::RegisterResource(Handle resource, RESOURCE_TYPE type, const ResourceUsage& usage)
	{
		const u64 resourceID = HashResource(resource, type);
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
			// TODO - Defer physical resource creation until baking?
			RenderResource newPhysicalResource{};
			newPhysicalResource.handle = resource;
			newPhysicalResource.resourceID = resourceID;
			newPhysicalResource.type = type;

			physicalResourceIndex = static_cast<u8>(m_physicalResources.size());
			m_physicalResources.push_back(newPhysicalResource);
		}

		// Create logical resource
		ResourceUsage newUsage = usage;
		newUsage.resourceID = resourceID;
		m_resourceUsages.push_back(newUsage);

		return physicalResourceIndex;
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
		// Traverse dependency tree and tag all passes which contribute to the final pass.
		// A pass reachable via multiple dependency paths (diamond-shaped graphs) is visited
		// more than once by the DFS, so de-duplicate here to avoid processing/executing it twice.
		const u32 passCount = static_cast<u32>(m_registeredRenderPasses.size());
		std::vector<bool> alreadyActive(passCount, false);

		TraverseDependencyTree(finalPassIndex, [&](const RenderPassVk& currRenderPass)
		{
			const u32 passIndex = currRenderPass.m_index;
			if (passIndex < passCount && !alreadyActive[passIndex])
			{
				alreadyActive[passIndex] = true;
				out_activeRenderPasses.push_back(passIndex);
			}
		});
	}

	void RenderGraphVk::CalculateResourceBarriers(const std::vector<u32>& activeRenderPasses, u32 finalPassIndex)
	{
		// Traverse the dependency tree from bottom-to-top, and for every dependency:
		// 1. Find which resource usages caused that dependency
		// 2. For all those resource usages, generate a barrier. Only generate pipeline barriers for now, and ignore cross-queue synchronization
		for (u32 activeRenderPassIndex : activeRenderPasses)
		{
			RenderPassVk& dstRenderPass = m_registeredRenderPasses[activeRenderPassIndex];
			const BIND_POINT dstBindPoint = dstRenderPass.m_bindPoint;

			// The final (backbuffer) pass needs explicit output transitions for the backbuffer
			// (-> PRESENT) and depth buffer, since no later pass depends on them to fill out the transition
			const bool isFinalPass = (dstRenderPass.m_index == finalPassIndex);
			if (isFinalPass)
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

					const VkImageLayout layout = CalculateResourceImageLayout(*dstResourceUsage, dstBindPoint);

					if (!isBackBufferResource && !isDepthBufferResource)
					{
						// Not a special resource - no output barrier needed for the last pass
						return;
					}

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

			// Non-graphics passes (transfer/compute) write their output textures directly (e.g. via a
			// buffer-to-image copy), so they require an explicit transition into the write layout
			// (e.g. TRANSFER_DST_OPTIMAL) before execution. Graphics passes get this implicitly from the
			// VkRenderPass' initialLayout/finalLayout, so they're skipped here.
			if (dstBindPoint != BIND_POINT::GRAPHICS)
			{
				TraverseRenderPassOutputs(dstRenderPass.m_index, [&](const RenderResource& resource)
				{
					if (resource.type != RESOURCE_TYPE::TEXTURE)
					{
						return;
					}
					TextureVk* pTexture = ResolveTexture(resource);
					ASSERT_PTR(pTexture);

					const u64& resourceID = resource.resourceID;
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(dstRenderPass, resourceID);
					ASSERT_PTR(dstResourceUsage); // Should never be null

					const VkImageLayout srcLayout = pTexture->GetLayout();
					const VkImageLayout dstLayout = CalculateResourceImageLayout(*dstResourceUsage, dstBindPoint);
					if (srcLayout != dstLayout)
					{
						Barrier newDstBarrier;
						newDstBarrier.dstAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
						newDstBarrier.srcAccessMask = 0; // TOP_OF_PIPE cannot have a non-zero access mask
						newDstBarrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
						newDstBarrier.dstStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.dstAccessMask, false);
						newDstBarrier.oldLayout = srcLayout;
						newDstBarrier.newLayout = dstLayout;

						dstRenderPass.m_inputBarriers.insert({ resourceID, newDstBarrier });
					}
				});
			}

			// Transition input textures that have NO producing dependency (root inputs) from whatever
			// layout they were left in (previous frame / external upload) to this pass' usage layout.
			// Inputs supplied by a dependency are handled by the dependency barrier loop below.
			ResourceIndexBitset coveredResources;
			for (const DependencyInfo& dependencyInfo : dstRenderPass.m_dependencyInfos)
			{
				coveredResources |= dependencyInfo.resources;
			}

			const ResourceIndexBitset rootInputResources = (dstRenderPass.m_inputResources & ~coveredResources);
			TraverseResources(rootInputResources, [&](const RenderResource& resource)
			{
				if (resource.type != RESOURCE_TYPE::TEXTURE)
				{
					return;
				}
				TextureVk* pTexture = ResolveTexture(resource);
				ASSERT_PTR(pTexture);

				const u64& resourceID = resource.resourceID;
				const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(dstRenderPass, resourceID);
				ASSERT_PTR(dstResourceUsage); // Should never be null

				// Only insert barriers if the layout is not compatible with the render passes' usage
				const VkImageLayout srcLayout = pTexture->GetLayout();
				const VkImageLayout dstLayout = CalculateResourceImageLayout(*dstResourceUsage, dstBindPoint);
				if (srcLayout != dstLayout)
				{
					Barrier newDstBarrier;
					newDstBarrier.srcAccessMask = 0; // TOP_OF_PIPE cannot have a non-zero access mask
					newDstBarrier.dstAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
					newDstBarrier.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					newDstBarrier.dstStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.dstAccessMask, false);
					newDstBarrier.oldLayout = srcLayout;
					newDstBarrier.newLayout = dstLayout;

					dstRenderPass.m_inputBarriers.insert({ resourceID, newDstBarrier });
				}
			});

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
		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetDeviceContext());

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
				BufferVk* pBuffer = ResolveBuffer(*resourceBarrier);
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
				TextureVk* pTexture = ResolveTexture(*resourceBarrier);
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

			TextureVk* textureResource = ResolveTexture(*resource);
			ASSERT_PTR(textureResource);

			textureResource->SetLayout(barrierInfo.newLayout);
		}
	}

	TextureVk* RenderGraphVk::ResolveTexture(const RenderResource& resource)
	{
		ASSERT_MSG(resource.type == RESOURCE_TYPE::TEXTURE, "Failed to resolve resource into texture. Resource is not a texture type!");

		const TextureHandle handle = static_cast<TextureHandle>(resource.handle);
		TextureVk* pTexture = static_cast<TextureVk*>(m_pRenderDevice->ResolveHandle(handle));
		return pTexture;
	}

	BufferVk* RenderGraphVk::ResolveBuffer(const RenderResource& resource)
	{
		ASSERT_MSG(resource.type == RESOURCE_TYPE::BUFFER, "Failed to resolve resource into buffer. Resource is not a buffer type!");

		const BufferHandle handle = static_cast<const BufferHandle>(resource.handle);
		BufferVk* pBuffer = static_cast<BufferVk*>(m_pRenderDevice->ResolveHandle(handle));
		return pBuffer;
	}

	const char* RenderGraphVk::GetResourceName(const RenderResource& resource)
	{
		switch (resource.type)
		{
		case RESOURCE_TYPE::TEXTURE:
		{
			TextureVk* pTexture = ResolveTexture(resource);
			if (pTexture != nullptr)
			{
				return pTexture->GetName();
			}
			break;
		}
		case RESOURCE_TYPE::BUFFER:
		{
			BufferVk* pBuffer = ResolveBuffer(resource);
			if (pBuffer != nullptr)
			{
				return pBuffer->GetName();
			}
			break;
		}
		case RESOURCE_TYPE::UNIFORM:
		{
			break;
		}
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to get resource name. Unknown resource type!");
		return nullptr;
	}
}