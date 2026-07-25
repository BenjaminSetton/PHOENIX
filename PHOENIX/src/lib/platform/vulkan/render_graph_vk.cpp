
#include <sstream>
#include <vulkan/vk_enum_string_helper.h>

#include "render_graph_vk.h"

#include "acceleration_structure_vk.h"
#include "buffer_vk.h"
#include "core/handle/handle_accessor.h"
#include "core/handle/handle_utils.h"
#include "core/global_settings.h"
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
	static const char* s_pReservedDepthBufferName = "INTERNAL_depthbuffer";
	static constexpr u32 s_invalidRenderPassIndex = U32_MAX;

	static u64 HashResource(Handle resource, const RESOURCE_TYPE& type)
	{
		size_t seed = 0;
		HashCombine(seed, resource.GetIndex());
		HashCombine(seed, type);

		return static_cast<u64>(seed);
	}

	static QUEUE_TYPE ConvertPassTypeToQueueType(PASS_TYPE passType)
	{
		switch (passType)
		{
		case PASS_TYPE::COMPUTE:      return QUEUE_TYPE::COMPUTE;
		case PASS_TYPE::GRAPHICS:     return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::TRANSFER:     return QUEUE_TYPE::TRANSFER;
		case PASS_TYPE::RAY_TRACING:  return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::AS_BUILD:     return QUEUE_TYPE::GRAPHICS;
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to convert pass type to queue type!");
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
		default:
		{
			break;
		}
		}

		LogError("Failed to calculate attachment type from texture's aspect flags. No valid combination was found for aspect flags %u", aspectFlags);
		return ATTACHMENT_TYPE::INVALID;
	}

	static VkAccessFlags CalculateResourceAccessFlags(const ResourceUsage& usage, const RenderResource& resource, PASS_TYPE passType)
	{
		// Acceleration structures use their own access flags regardless of bind point
		if (resource.type == RESOURCE_TYPE::ACCELERATION_STRUCTURE)
		{
			return (usage.io == RESOURCE_IO::INPUT) ? VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR : VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		}

		VkAccessFlags flags = 0;
		switch (usage.io)
		{
		case RESOURCE_IO::INPUT:
		{
			if (passType == PASS_TYPE::TRANSFER)
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
					case BUFFER_USAGE::ACCELERATION_STRUCTURE_BUILD_INPUT:
					{
						if (passType == PASS_TYPE::AS_BUILD)
						{
							flags |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
						}
						else
						{
							// These buffers have STORAGE_BUFFER_BIT and may be read as storage buffers in shaders
							flags |= VK_ACCESS_SHADER_READ_BIT;
						}
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
					flags |= VK_ACCESS_SHADER_READ_BIT;
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
			if (passType == PASS_TYPE::TRANSFER || passType == PASS_TYPE::AS_BUILD)
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
					if (passType == PASS_TYPE::RAY_TRACING || passType == PASS_TYPE::COMPUTE)
					{
						flags |= VK_ACCESS_SHADER_WRITE_BIT;
						break;
					}

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

	static VkPipelineStageFlags CalculateResourcePipelineStageFlags(PASS_TYPE passType, VkAccessFlags accessFlag, bool isSrcFlag)
	{
		if (accessFlag == 0)
		{
			return (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		}

		// Acceleration structure access flags are independent of the pass bind point
		if (accessFlag == VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		{
			return (passType == PASS_TYPE::RAY_TRACING) ? VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR : VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		}
		if (accessFlag == VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR || accessFlag == VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		{
			return VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		}

		VkPipelineStageFlags flags = 0;
		switch (passType)
		{
		case PASS_TYPE::GRAPHICS:
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
				// TODO - Find a way to tell which shader is writing to the resource so we don't have to include
				//        all the shader flags. For now, we're only covering vertex and fragment shaders, but realistically
				//        this should cover more shader types like geometry, tesselation, etc but I don't want to
				//        guard against all of them here
				flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
		case PASS_TYPE::COMPUTE:
		{
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		}
		case PASS_TYPE::TRANSFER:
		{
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		}
		case PASS_TYPE::RAY_TRACING:
		{
			flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		}
		case PASS_TYPE::AS_BUILD:
		{
			flags |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to calculate resource pipeline stage flags. Unknown pass type!");
		}
		}

		return flags;
	}

	static VkImageLayout CalculateResourceImageLayout(const ResourceUsage& usage, PASS_TYPE passType)
	{
		switch (usage.io)
		{
		case RESOURCE_IO::INPUT:
		{
			switch (passType)
			{
			case PASS_TYPE::GRAPHICS:
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
			case PASS_TYPE::COMPUTE:
			{
				return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			case PASS_TYPE::TRANSFER:
			{
				return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			}
			case PASS_TYPE::RAY_TRACING:
			{
				return VK_IMAGE_LAYOUT_GENERAL;
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
			switch (passType)
			{
			case PASS_TYPE::GRAPHICS:
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
			case PASS_TYPE::COMPUTE:
			{
				return VK_IMAGE_LAYOUT_GENERAL;
			}
			case PASS_TYPE::TRANSFER:
			{
				return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			}
			case PASS_TYPE::RAY_TRACING:
			{
				return VK_IMAGE_LAYOUT_GENERAL;
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

	RenderPassVk::RenderPassVk(const char* name, PASS_TYPE passType, u32 index, RegisterResourceCallbackFn registerResourceCallback) : 
		m_passType(passType), m_registerResourceCallback(registerResourceCallback), m_index(index)
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
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;
		usage.attachmentType = CalculateAttachmentType(texture);
		usage.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::LOAD;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferInput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;
		usage.bufferUsage = buffer.GetUsage();

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetUniformInput(UniformCollectionHandle uniformCollection)
	{
		TODO();
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(uniformCollection, RESOURCE_TYPE::UNIFORM, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetAccelerationStructureInput(AccelerationStructureHandle accelerationStructure)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::INPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(accelerationStructure, RESOURCE_TYPE::ACCELERATION_STRUCTURE, usage);
		m_inputResources.set(resourceIndex);
	}

	void RenderPassVk::SetColorOutput(TextureHandle texture)
	{
		SetTextureOutput(texture, ATTACHMENT_LOAD_OP::CLEAR, ATTACHMENT_STORE_OP::STORE, {});
	}

	void RenderPassVk::SetDepthOutput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;
		usage.clearValue.useClearColor = false;
		usage.clearValue.depthStencil.depthClear = 1.0f;
		usage.clearValue.depthStencil.stencilClear = 0;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthStencilOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH_STENCIL;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetResolveOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::RESOLVE;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	// NOTE: Ordering guarantee - passes that write the same texture execute in registration order.
	// This is a guaranteed property of the render graph: BuildDependencyTree() only creates
	// dependencies pointing to earlier-registered passes, and WAW hazards are included. So any two
	// passes writing the same output are forced into submission order by the backward-only scan.
	void RenderPassVk::SetTextureOutput(TextureHandle texture, ATTACHMENT_LOAD_OP loadOp, ATTACHMENT_STORE_OP storeOp, ClearValues clearValue)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.attachmentType = CalculateAttachmentType(texture);
		usage.storeOp = storeOp;
		usage.loadOp = loadOp;
		usage.clearValue = clearValue;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferOutput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;
		usage.bufferUsage = buffer.GetUsage();

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
		m_outputResources.set(resourceIndex);
	}

	void RenderPassVk::SetAccelerationStructureOutput(AccelerationStructureHandle accelerationStructure)
	{
		ResourceUsage usage{};
		usage.io = RESOURCE_IO::OUTPUT;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(accelerationStructure, RESOURCE_TYPE::ACCELERATION_STRUCTURE, usage);
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

	void RenderPassVk::SetPipelineDescription(const RayTracingPipelineDesc& rayTracingPipelineDesc)
	{
		rayTracingDesc = rayTracingPipelineDesc;
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

	RenderGraphVk::RenderGraphVk(RenderDeviceVk* pRenderDevice) : m_pRenderDevice(nullptr), m_deviceContextHandles(), m_currentFrameGraphHash(0), m_uniqueVisualizationHashes(),
		m_frameInFlightIndex(0), m_frameNumber(0), m_reservedDepthBufferNameCRC(HashCRC32(s_pReservedDepthBufferName)), m_presentResID(0), m_didExecuteWork(false),
		m_metrics(), m_queryPool(VK_NULL_HANDLE), m_timestampPeriod(0.0f)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to initialize render graph. Render device is null!");
			return;
		}

		m_pRenderDevice = pRenderDevice;

		// Device contexts
		const u32 framesInFlight = m_pRenderDevice->GetFramesInFlight();
		for (u32 i = 0; i < framesInFlight; i++)
		{
			DeviceContextCreateInfo deviceContextCI{};
			deviceContextCI.assignedFrameIndex = i;

			DeviceContextHandle deviceContext;
			STATUS_CODE res = m_pRenderDevice->AllocateDeviceContext(deviceContextCI, deviceContext);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to construct render graph. Device context creation failed!");
				return;
			}
			m_deviceContextHandles.push_back(deviceContext);
		}

		// Create timestamp query pool for GPU frame time metrics
		if (GetSettings().gatherMetrics)
		{
			m_timestampPeriod = static_cast<float>(m_pRenderDevice->GetDeviceProperties().limits.timestampPeriod);

			VkQueryPoolCreateInfo queryPoolCI{};
			queryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
			// TODO: Look into using VK_QUERY_TYPE_PIPELINE_STATISTICS for pipeline stats (vertex shader invocations, clipping primitives, etc.)
			queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
			queryPoolCI.queryCount = framesInFlight * 2; // 2 queries per frame-in-flight (begin + end)

			VkResult vkRes = vkCreateQueryPool(m_pRenderDevice->GetLogicalDevice(), &queryPoolCI, nullptr, &m_queryPool);
			if (vkRes != VK_SUCCESS)
			{
				LogError("Failed to create timestamp query pool for metrics. Got error: \"%s\"", string_VkResult(vkRes));
				m_queryPool = VK_NULL_HANDLE;
			}
		}
	}

	RenderGraphVk::~RenderGraphVk()
	{
		m_deviceContextHandles.clear();
		m_registeredRenderPasses.DeleteAll();

		if (m_queryPool != VK_NULL_HANDLE)
		{
			vkDestroyQueryPool(m_pRenderDevice->GetLogicalDevice(), m_queryPool, nullptr);
			m_queryPool = VK_NULL_HANDLE;
		}
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

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());
		res = pDeviceContext->BeginFrame(swapChainVk);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame. Device context could not begin frame!");
			return res;
		}

		if (GetSettings().gatherMetrics)
		{
			// Reset all per-frame metrics to default values
			m_metrics = Metrics{};

			// Read back timestamp query results from the previous frame.
			// Guarded by m_didExecuteWork so that we can safely wait on
			// the query results if work was submitted, otherwise no waiting is done
			const bool canQueryResults = m_queryPool != VK_NULL_HANDLE && m_frameNumber > 0;
			if (m_didExecuteWork && canQueryResults)
			{
				u32 prevFrameIndex = (m_frameInFlightIndex == 0) ? (m_pRenderDevice->GetFramesInFlight() - 1) : (m_frameInFlightIndex - 1);
				u64 timestamps[2] = { 0, 0 };
				VkResult vkRes = vkGetQueryPoolResults(
					m_pRenderDevice->GetLogicalDevice(),
					m_queryPool,
					prevFrameIndex * 2,
					2,
					sizeof(timestamps),
					timestamps,
					sizeof(u64),
					VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

				if (vkRes == VK_SUCCESS)
				{
					u64 diff = timestamps[1] - timestamps[0];
					m_metrics.gpuFrameTime = static_cast<float>(diff) * m_timestampPeriod / 1e6f;
				}
			}
		}

		m_didExecuteWork = false;

		return res;
	}

	STATUS_CODE RenderGraphVk::EndFrame(SwapChainHandle swapChain)
	{
		if (!swapChain.IsValid())
		{
			LogError("Failed to begin frame. Swap chain handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(m_pRenderDevice->ResolveHandle(swapChain));
		ASSERT_PTR(swapChainVk);

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());

		// Write end-of-frame timestamp before submission so it's recorded in the command buffer.
		// This writes into the last command buffer (BOTTOM_OF_PIPE), which only executes after
		// all previous batches complete
		if (GetSettings().gatherMetrics && m_queryPool != VK_NULL_HANDLE)
		{
			pDeviceContext->WriteEndTimestamp();
		}

		res = pDeviceContext->EndFrame();
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame #%u. Device context could not flush!", m_frameNumber);
		}

		// Present
		res = swapChainVk->Present(m_frameInFlightIndex);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame #%u. Swap chain present failed!", m_frameNumber);
		}

		// Now that all the work has been done for the current frame, move onto the next one
		m_frameInFlightIndex = (m_frameInFlightIndex + 1) % m_pRenderDevice->GetFramesInFlight();
		m_frameNumber++;

		m_registeredRenderPasses.DeleteAll();

		m_resourceUsages.clear();
		m_physicalResources.clear();

		return res;
	}

	STATUS_CODE RenderGraphVk::RegisterPass(const char* passName, PASS_TYPE passType, RenderPassHandle& renderPass)
	{
		// TODO - Reconcile with HANDLE_UTILS functions
		auto registerResourceFuncPtr = std::bind(&RenderGraphVk::RegisterResource, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

		RenderPassVk* newRenderPass = new RenderPassVk(passName, passType, 0u, registerResourceFuncPtr);
		const u32 passIndex = m_registeredRenderPasses.Allocate(newRenderPass);
		newRenderPass->m_index = passIndex;

		// NOTE - Manually call PopulateHandle() from HandleAccessor vs using HANDLE_UTILS, 
		// since that inserts an InterfaceT pointer into an array
		HandleAccessor::PopulateHandle(renderPass, this, passIndex);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderGraphVk::Bake(SwapChainHandle swapChain)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;

		// Resolve the current swapchain image and compute its resource ID.
		// All passes writing the swapchain target this same handle for the current frame.
		TextureHandle currentImage = swapChain.GetCurrentImage();
		m_presentResID = HashResource(currentImage, RESOURCE_TYPE::TEXTURE);

		// Create the render graph tree using the following steps:
		// 1. Find the render pass that owns presentation (last pass writing to the swapchain image)
		u32 finalRPIndex = FindPresentRenderPassIndex(m_presentResID);
		if (finalRPIndex == s_invalidRenderPassIndex)
		{
			LogError("Failed to bake render graph. No render pass writes to the swapchain image!");
			return STATUS_CODE::ERR_INTERNAL;
		}
		
		// 2. Once that render pass is found, build the dependency tree
		BuildDependencyTree(finalRPIndex);

		// 3. [TRIMMING] Accumulate all contributing render passes into a separate container for the render graph. This is done so that
		//               all non-contributing passes are indirectly trimmed
		std::vector<u32> activeRenderPassIndices;
		activeRenderPassIndices.reserve(m_registeredRenderPasses.Size());

		FindActivePasses(finalRPIndex, activeRenderPassIndices);

		CalculateResourceBarriers(activeRenderPassIndices, finalRPIndex);

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO
		
		// Now that the render graph has been generated, run through it and perform the following steps for each render pass:
		// 1. Declare the resources that will get used in the device context
		// 2. Insert resource barriers and/or perform layout transitions as necessary
		// 3. Call the execute callback and pass in the device context
		std::reverse(activeRenderPassIndices.begin(), activeRenderPassIndices.end());

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());
		DeviceContextHandle deviceContext = GetCurrentDeviceContextHandle();

		// Inject metrics pointer and query pool so draw/uniform stats are accumulated during pass execution
		// and GPU timestamps bracket the entire frame's command buffer workload
		if (GetSettings().gatherMetrics)
		{
			pDeviceContext->SetMetricsPointer(&m_metrics);
			if (m_queryPool != VK_NULL_HANDLE)
			{
				pDeviceContext->SetQueryPool(m_queryPool, m_frameInFlightIndex * 2);
			}
		}

		for (u32 activeRenderPassIndex : activeRenderPassIndices)
		{
			const RenderPassVk& currRenderPass = *m_registeredRenderPasses.Get(activeRenderPassIndex);

			// Before calling execution callback, insert all barriers required by the render pass
			res = InsertResourceBarriers(currRenderPass);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to bake render graph. Could not insert dependency barriers!");
				return res;
			}

			// Insert a label for GPU operations
			{
				const QUEUE_TYPE passQueueType = ConvertPassTypeToQueueType(currRenderPass.m_passType);
#if defined(PHX_DEBUG)
				const char* passName = currRenderPass.m_debugName;
#else
				const char* passName = "UnnamedPass";
#endif
				pDeviceContext->BeginLabel(passQueueType, passName);
			}

			switch (currRenderPass.m_passType)
			{
				case PASS_TYPE::GRAPHICS:
				{
					// Get or create render pass (refers to internal cache)
					VkRenderPass renderPassVk = CreateRenderPass(currRenderPass);
		
					// Get or create framebuffer from render device (refers to internal cache)
					// isBackbuffer: true if this pass writes the swapchain image (triggers resize invalidation)
					const bool isBackbuffer = PassWritesResource(currRenderPass.m_index, m_presentResID);
					FramebufferVk* pFramebuffer = CreateFramebuffer(currRenderPass, renderPassVk, isBackbuffer);

					// Build per-attachment clear values from each output's ResourceUsage.clearValue
					std::vector<ClearValues> clearValues;
					TraverseRenderPassOutputs(currRenderPass.m_index, [&](const RenderResource& resource)
					{
						if (resource.type != RESOURCE_TYPE::TEXTURE)
						{
							return;
						}
						const ResourceUsage* usage = GetResourceUsageFromPass(currRenderPass, resource.resourceID);
						if (usage != nullptr)
						{
							clearValues.push_back(usage->clearValue);
						}
					});

					res = pDeviceContext->BeginRenderPass(renderPassVk, pFramebuffer, clearValues.data(), static_cast<u32>(clearValues.size()));
					if (res != STATUS_CODE::SUCCESS)
					{
						LogError("Failed to bake render pass. Device context could not begin render pass!");
						return res;
					}

					// Determine if this pass has a pipeline description. Clear-only passes
					// register as graphics passes with texture outputs but no shaders, so they only need the
					// render pass begin/end to perform attachment clears.
					const bool hasPipeline = (currRenderPass.graphicsDesc.shaderCount > 0 && currRenderPass.graphicsDesc.pShaders != nullptr);

					if (hasPipeline)
					{
						PipelineVk* pPipeline = CreatePipeline(currRenderPass, renderPassVk);
						pDeviceContext->SetContextualPipeline(pPipeline);
					}

					CallExecutionCallback(currRenderPass, deviceContext);

					if (hasPipeline)
					{
						pDeviceContext->ResetContextualPipeline();
					}

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
				case PASS_TYPE::COMPUTE:
				{
					// Get or create pipeline from render device (refes to internal cache)
					// NOTE - The render pass isn't used for compute pipeline creation, so it can
					// be ignored by passing in VK_NULL_HANDLE
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, VK_NULL_HANDLE);

					pDeviceContext->SetContextualPipeline(pPipeline);
					CallExecutionCallback(currRenderPass, deviceContext);
					pDeviceContext->ResetContextualPipeline();

					break;
				}
				case PASS_TYPE::TRANSFER:
				{
					// Transfer-only passes do not use a pipeline
					CallExecutionCallback(currRenderPass, deviceContext);

					break;
				}
				case PASS_TYPE::RAY_TRACING:
				{
					// Get or create pipeline from render device
					// NOTE - The render pass isn't used for ray tracing pipeline creation, so it can
					// be ignored by passing in VK_NULL_HANDLE
					PipelineVk* pPipeline = CreatePipeline(currRenderPass, VK_NULL_HANDLE);

					pDeviceContext->SetContextualPipeline(pPipeline);
					CallExecutionCallback(currRenderPass, deviceContext);
					pDeviceContext->ResetContextualPipeline();

					// Update the layout of the render pass' textures to reflect the implicit
					// layout transition from the render pass
					UpdateTextureLayouts(activeRenderPassIndex);

					break;
				}
				case PASS_TYPE::AS_BUILD:
				{
					// AS build passes do not use a pipeline or render pass — just execute the callback
					CallExecutionCallback(currRenderPass, deviceContext);

					break;
				}
			}

			// End the label for this pass
			pDeviceContext->EndLabel(ConvertPassTypeToQueueType(currRenderPass.m_passType));
		}


		// Clear metrics pointer after pass execution
		if (GetSettings().gatherMetrics)
		{
			pDeviceContext->ResetMetricsPointer();
			m_metrics.passCount = static_cast<u32>(activeRenderPassIndices.size());
		}

		// Hash the state of the render graph after baking
		// and store it in the map
		m_currentFrameGraphHash = HashState();

		return res;
	}

	u32 RenderGraphVk::GetFrameNumber() const
	{
		return m_frameNumber;
	}

	const Metrics& RenderGraphVk::GetMetrics() const
	{
		if (!GetSettings().gatherMetrics)
		{
			static Metrics s_defaultMetrics{};
			return s_defaultMetrics;
		}

		// Query device for resource handle counts and allocated memory at call time
		m_metrics.bufferCount = m_pRenderDevice->GetBufferCount();
		m_metrics.textureCount = m_pRenderDevice->GetTextureCount();
		m_metrics.shaderCount = m_pRenderDevice->GetShaderCount();
		m_metrics.pipelineCount = m_pRenderDevice->GetPipelineCount();
		m_metrics.uniformCollectionCount = m_pRenderDevice->GetUniformCollectionCount();
		m_metrics.accelerationStructureCount = m_pRenderDevice->GetAccelerationStructureCount();
		m_metrics.allocatedMemoryBytes = m_pRenderDevice->GetAllocatedMemoryBytes();

		return m_metrics;
	}

	STATUS_CODE RenderGraphVk::GenerateVisualization(const char* fileName, bool generateIfUnique)
	{
		if (fileName == nullptr)
		{
			LogError("Failed to generate render graph visualization. File name is null!");
			return STATUS_CODE::ERR_API;
		}

		if (m_registeredRenderPasses.Empty())
		{
			LogError("Failed to generate render graph visualization. No render passes registered - call after Bake()!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (generateIfUnique)
		{
			auto hashIter = m_uniqueVisualizationHashes.find(m_currentFrameGraphHash);
			if (hashIter != m_uniqueVisualizationHashes.end())
			{
				// We've already generated a visualization for the current state. Don't re-generate
				return STATUS_CODE::SUCCESS;
			}
		}

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
		for (u32 i = 0; i < m_registeredRenderPasses.Size(); i++)
		{
			const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(i);
			if (pRenderPass == nullptr) 
			{
				continue;
			}

#if defined(PHX_DEBUG)
			const char* passName = pRenderPass->m_debugName;
#else
			const std::string passNameStr = std::to_string(pRenderPass->m_index);
			const char* passName = passNameStr.c_str();
#endif
			const char* passTypeStr = RG_UTILS::PassTypeToString(pRenderPass->m_passType);

			const char* fillColor = "#FFFFFF";
			if (pRenderPass->m_passType == PASS_TYPE::GRAPHICS)       fillColor = "#5DADE2";
			else if (pRenderPass->m_passType == PASS_TYPE::COMPUTE)   fillColor = "#58D68D";
			else if (pRenderPass->m_passType == PASS_TYPE::TRANSFER)  fillColor = "#EB984E";
			else if (pRenderPass->m_passType == PASS_TYPE::RAY_TRACING) fillColor = "#AF7AC5";
			else if (pRenderPass->m_passType == PASS_TYPE::AS_BUILD)  fillColor = "#48C9B0";

			const bool isFinalPass = PassWritesResource(pRenderPass->m_index, m_presentResID);

			dot << "\tpass" << pRenderPass->m_index
				<< " [shape=box, style=\"filled,rounded\", fontcolor=\"#FFFFFF\", margin=\"0.25,0.14\""
				<< ", fillcolor=\"" << fillColor << "\"";
			if (isFinalPass)  dot << ", penwidth=3, color=\"#C0392B\"";
			else              dot << ", penwidth=1, color=\"#34495E\"";

			dot << ", label=<<b>" << passName << "</b><br/><font point-size=\"9\">" << passTypeStr;
			if (isFinalPass)  dot << " &#8226; FINAL";
			dot << "</font>>];\n";
		}

		dot << "\n";

		// ---- Resource nodes ----
		// Gather every physical resource referenced by any pass (inputs or outputs)
		ResourceIndexBitset usedResources;
		for (u32 i = 0; i < m_registeredRenderPasses.Size(); i++)
		{
			const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(i);
			if (pRenderPass == nullptr) 
			{
				continue;
			}
			
			usedResources |= pRenderPass->m_inputResources;
			usedResources |= pRenderPass->m_outputResources;
		}

		dot << "\t// Resources\n";
		TraverseResources(usedResources, [&](const RenderResource& resource)
		{
			const std::string nodeId = "res" + std::to_string(resource.resourceID);
			const bool isBackbuffer = (resource.resourceID == m_presentResID);

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
			else if (resource.type == RESOURCE_TYPE::ACCELERATION_STRUCTURE)
			{
				displayName = GetResourceName(resource);
				typeTag = "ACCELERATION STRUCTURE";
				fill    = "#E8F8F5";
				border  = "#1ABC9C";
			}
			else // TEXTURE
			{
				displayName = GetResourceName(resource);
			}

			if (isBackbuffer)
			{
				typeTag = "PRESENT";
				fill    = "#FADBD8";
				border  = "#C0392B";
				penWidth = 3;
			}

			dot << "\t" << nodeId << " [shape=ellipse, style=filled, fillcolor=\"" << fill
				<< "\", color=\"" << border << "\", penwidth=" << penWidth
				<< ", label=<<b>" << displayName << "</b><br/><font point-size=\"8\" color=\"#5D6D7E\">" << typeTag << "</font>>];\n";
		});

		dot << "\n\t// Resource flow (inputs feed passes, passes produce outputs)\n";

		// ---- Edges: resource -> pass (inputs) and pass -> resource (outputs) ----
		for (u32 i = 0; i < m_registeredRenderPasses.Size(); i++)
		{
			const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(i);
			if (pRenderPass == nullptr) 
			{
				continue;
			}

			const std::string passNode = "pass" + std::to_string(pRenderPass->m_index);

			// Inputs: resource -> pass (blue), labelled with the input layout transition for textures
			TraverseResources(pRenderPass->m_inputResources, [&](const RenderResource& resource)
			{
				const std::string resNode = "res" + std::to_string(resource.resourceID);

				std::string label;
				std::string tooltip;
				auto barrierIter = pRenderPass->m_inputBarriers.find(resource.resourceID);
				if (barrierIter != pRenderPass->m_inputBarriers.end())
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
			TraverseResources(pRenderPass->m_outputResources, [&](const RenderResource& resource)
			{
				const std::string resNode = "res" + std::to_string(resource.resourceID);

				std::string label;
				std::string tooltip;
				auto barrierIter = pRenderPass->m_outputBarriers.find(resource.resourceID);
				if (barrierIter != pRenderPass->m_outputBarriers.end())
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
		dot << "\t\t<TR><TD BGCOLOR=\"#AF7AC5\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Ray tracing pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"#48C9B0\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">AS build pass</TD></TR>\n";

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

		// Cache the hash that we've generated a visualization for
		m_uniqueVisualizationHashes.insert({ m_currentFrameGraphHash, true });

		LogDebug("Render graph visualization written to \"%s\" for hash %llX", fileName, m_currentFrameGraphHash);
		return STATUS_CODE::SUCCESS;
	}

	IDeviceContext* RenderGraphVk::GetCurrentDeviceContext()
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

	DeviceContextHandle RenderGraphVk::GetCurrentDeviceContextHandle()
	{
		ASSERT(m_frameInFlightIndex < m_deviceContextHandles.size());
		return m_deviceContextHandles[m_frameInFlightIndex];
	}

	void* RenderGraphVk::ResolveHandle(const Handle& handle)
	{
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS: return m_registeredRenderPasses.Resolve(handle.GetIndex());
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
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS:
		{
			m_registeredRenderPasses.IncrementRefCount(handle.GetIndex());
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
		const HANDLE_TYPE type = handle.GetType();
		switch (type)
		{
		case HANDLE_TYPE::RENDER_PASS:
		{
			m_registeredRenderPasses.DecrementRefCountNoDelete(handle.GetIndex());
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
		subpassDesc.bindPoint = RG_UTILS::ConvertPassTypeToBindPoint(renderPass.m_passType);

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
				// means that it's the backbuffer pass since no other pass depends on it
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

		// May return cached render pass if a match is found
		VkRenderPass renderPassVk = m_pRenderDevice->GetOrCreateRenderPass(renderPassDesc);
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

		PASS_TYPE renderPassBindPoint = renderPass.m_passType;
		switch (renderPassBindPoint)
		{
		case PASS_TYPE::GRAPHICS:
		{
			pipeline = m_pRenderDevice->CreateGraphicsPipeline(renderPass.graphicsDesc, renderPassVk);
			break;
		}
		case PASS_TYPE::COMPUTE:
		{
			pipeline = m_pRenderDevice->CreateComputePipeline(renderPass.computeDesc);
			break;
		}
		case PASS_TYPE::RAY_TRACING:
		{
			pipeline = m_pRenderDevice->CreateRayTracingPipeline(renderPass.rayTracingDesc);
			break;
		}
		case PASS_TYPE::TRANSFER:
		case PASS_TYPE::AS_BUILD:
		{
			// Transfer and AS build passes do not use pipelines
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Unknown pass type!");
			break;
		}
		}

		return pipeline;
	}

	ResourceIndex RenderGraphVk::RegisterResource(Handle resource, RESOURCE_TYPE type, const ResourceUsage& usage)
	{
		const ResourceIndex numPhysicalResources = static_cast<ResourceIndex>(m_physicalResources.size());
		if (numPhysicalResources > MAX_REGISTERED_RESOURCES)
		{
			LogError("Failed to register resource. Physical resource limit (%u) reached!", MAX_REGISTERED_RESOURCES);
			return 0;
		}

		const u64 resourceID = HashResource(resource, type);
		ResourceIndex physicalResourceIndex = MAX_REGISTERED_RESOURCES;

		// Try to find an existing physical resource
		for (ResourceIndex i = 0; i < numPhysicalResources; i++)
		{
			const RenderResource& physicalResource = m_physicalResources[i];
			if (physicalResource.resourceID == resourceID)
			{
				// Found match
				physicalResourceIndex = i;
				break;
			}
		}
		
		if (physicalResourceIndex == MAX_REGISTERED_RESOURCES)
		{
			// Couldn't find existing physical resource, create one instead
			// TODO - Defer physical resource creation until baking?
			RenderResource newPhysicalResource{};
			newPhysicalResource.handle = resource;
			newPhysicalResource.resourceID = resourceID;
			newPhysicalResource.type = type;

			physicalResourceIndex = numPhysicalResources;
			m_physicalResources.push_back(newPhysicalResource);
		}

		// Create logical resource
		ResourceUsage newUsage = usage;
		newUsage.resourceID = resourceID;
		m_resourceUsages.push_back(newUsage);

		return physicalResourceIndex;
	}

	u32 RenderGraphVk::FindPresentRenderPassIndex(u64 presentResID)
	{
		u32 presentRPIndex = s_invalidRenderPassIndex;

		for (const ResourceUsage& usage : m_resourceUsages)
		{
			if (usage.io != RESOURCE_IO::OUTPUT)
			{
				continue;
			}

			if (usage.resourceID == presentResID)
			{
				// Track the last (highest passIndex) writer - it owns presentation
				if (presentRPIndex == s_invalidRenderPassIndex || usage.passIndex > presentRPIndex)
				{
					presentRPIndex = usage.passIndex;
				}
			}
		}

		return presentRPIndex;
	}

	bool RenderGraphVk::PassWritesResource(u32 renderPassIndex, u64 resourceID) const
	{
		for (const ResourceUsage& usage : m_resourceUsages)
		{
			if (usage.passIndex == renderPassIndex && usage.io == RESOURCE_IO::OUTPUT && usage.resourceID == resourceID)
			{
				return true;
			}
		}
		return false;
	}

	void RenderGraphVk::BuildDependencyTree(u32 renderPassIndex)
	{
		// Base cases
		if (renderPassIndex >= m_registeredRenderPasses.Size())
		{
			return;
		}

		RenderPassVk* pCurrRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		if (pCurrRenderPass->m_inputResources.none() && pCurrRenderPass->m_outputResources.none())
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

			RenderPassVk* pPrevRenderPass = m_registeredRenderPasses.Get(i);

			const ResourceIndexBitset rawHazardResources = (pPrevRenderPass->m_outputResources & pCurrRenderPass->m_inputResources);
			const ResourceIndexBitset warHazardResources = (pPrevRenderPass->m_inputResources  & pCurrRenderPass->m_outputResources);
			const ResourceIndexBitset wawHazardResources = (pPrevRenderPass->m_outputResources & pCurrRenderPass->m_outputResources);

			// Create a new dependency to this previous render pass if any hazards are detected
			const ResourceIndexBitset hazardResources = (rawHazardResources | warHazardResources | wawHazardResources);
			const bool isDependencyRP = hazardResources.any();
			if (isDependencyRP)
			{
				DependencyInfo newDependency{};
				newDependency.renderPass = pPrevRenderPass;
				newDependency.resources = hazardResources;
				pCurrRenderPass->m_dependencyInfos.push_back(newDependency);
				BuildDependencyTree(i);
			}
		}
	}

	void RenderGraphVk::FindActivePasses(u32 finalPassIndex, std::vector<u32>& out_activeRenderPasses)
	{
		// Traverse dependency tree and tag all passes which contribute to the final pass.
		// A pass reachable via multiple dependency paths (diamond-shaped graphs) is visited
		// more than once by the DFS, so de-duplicate here to avoid processing/executing it twice.
		const u32 passCount = static_cast<u32>(m_registeredRenderPasses.Size());
		std::vector<i32> alreadyActive(passCount, -1);

		TraverseDependencyTree(finalPassIndex, [&](const RenderPassVk& currRenderPass)
		{
			const u32 passIndex = currRenderPass.m_index;
			if (passIndex < passCount)
			{
				if (alreadyActive[passIndex] != -1)
				{
					// This means the same pass appeared as a dependency for an earlier pass
					// in submission order. Therefore we must push it to the back of the list
					i32 lastPassIndex = alreadyActive[passIndex];
					auto lastPassIndexIter = out_activeRenderPasses.begin() + lastPassIndex;
					std::rotate(lastPassIndexIter, lastPassIndexIter + 1, out_activeRenderPasses.end());
				}
				else
				{
					// Push back new active pass. In this case, alreadyActive holds the 
					// index of where the pass was last inserted
					alreadyActive[passIndex] = static_cast<i32>(out_activeRenderPasses.size());
					out_activeRenderPasses.push_back(passIndex);
				}

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
			RenderPassVk* pDstRenderPass = m_registeredRenderPasses.Get(activeRenderPassIndex);
			const PASS_TYPE dstBindPoint = pDstRenderPass->m_passType;

			// Every active pass needs an entry in m_outputBarriers for each of its texture outputs so
			// that CreateRenderPass can find the finalLayout for each attachment.
			//
			// Color attachments: only the final (backbuffer) pass needs the color->PRESENT transition.
			//   Non-final color outputs get their output barrier filled in by the dependency loop below
			//   (the next pass that writes the same color target will insert a WAW barrier entry there).
			//
			// Depth attachments: if no later pass reads/writes the depth buffer (the common case today),
			//   the dependency loop will never insert an output barrier for it. We generate a terminal
			//   depth output barrier here for every pass that writes depth so CreateRenderPass never
			//   hits the "Failed to find output barrier" assert.
			const bool isFinalPass = (pDstRenderPass->m_index == finalPassIndex);
			TraverseRenderPassOutputs(pDstRenderPass->m_index, [&](const RenderResource& resource)
			{
				if (resource.type != RESOURCE_TYPE::TEXTURE)
				{
					return;
				}

				const u64& resourceID = resource.resourceID;
				const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(*pDstRenderPass, resourceID);
				ASSERT_PTR(dstResourceUsage); // Should never be null

				const bool isColorAttachment = (dstResourceUsage->attachmentType == ATTACHMENT_TYPE::COLOR);

				// Color->PRESENT is only needed for the final pass's color output; skip for all others
				// (the dependency loop will supply the correct WAW output barrier for non-final writers).
				if (isColorAttachment && !isFinalPass)
				{
					return;
				}

				const VkImageLayout layout = CalculateResourceImageLayout(*dstResourceUsage, dstBindPoint);

				Barrier newDstBarrier;
				newDstBarrier.oldLayout = layout;
				newDstBarrier.newLayout = isColorAttachment ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : layout;

				// NOTE - Presentation engine is external and doesn't require an access mask, but if the backbuffer
				//        is cleared (from loadOp) we must still sync the clear operation. As a result, we'll set
				//        the dst flags to COLOR_ATTACHMENT-related write operations to be safe
				newDstBarrier.srcAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
				newDstBarrier.dstAccessMask = isColorAttachment ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
				newDstBarrier.srcStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.srcAccessMask, true);
				newDstBarrier.dstStageMask = isColorAttachment ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

				pDstRenderPass->m_outputBarriers.insert({ resourceID, newDstBarrier });
			});

			// Non-graphics passes (transfer/compute) write their output textures directly (e.g. via a
			// buffer-to-image copy), so they require an explicit transition into the write layout
			// (e.g. TRANSFER_DST_OPTIMAL) before execution. Graphics passes get this implicitly from the
			// VkRenderPass' initialLayout/finalLayout, so they're skipped here.
			if (dstBindPoint != PASS_TYPE::GRAPHICS)
			{
				TraverseRenderPassOutputs(pDstRenderPass->m_index, [&](const RenderResource& resource)
				{
					if (resource.type != RESOURCE_TYPE::TEXTURE)
					{
						return;
					}
					TextureVk* pTexture = ResolveTexture(resource);
					ASSERT_PTR(pTexture);

					const u64& resourceID = resource.resourceID;
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(*pDstRenderPass, resourceID);
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

						pDstRenderPass->m_inputBarriers.insert({ resourceID, newDstBarrier });
					}
				});
			}

			// Transition input textures that have NO producing dependency (root inputs) from whatever
			// layout they were left in (previous frame / external upload) to this pass' usage layout.
			// Inputs supplied by a dependency are handled by the dependency barrier loop below.
			ResourceIndexBitset coveredResources;
			for (const DependencyInfo& dependencyInfo : pDstRenderPass->m_dependencyInfos)
			{
				coveredResources |= dependencyInfo.resources;
			}

			const ResourceIndexBitset rootInputResources = (pDstRenderPass->m_inputResources & ~coveredResources);
			TraverseResources(rootInputResources, [&](const RenderResource& resource)
			{
				const u64& resourceID = resource.resourceID;
				const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(*pDstRenderPass, resourceID);
				ASSERT_PTR(dstResourceUsage); // Should never be null

				if (resource.type == RESOURCE_TYPE::ACCELERATION_STRUCTURE)
				{
					// Root AS resources are assumed to have been produced by an AS build in a previous frame
					Barrier newDstBarrier;
					newDstBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
					newDstBarrier.dstAccessMask = CalculateResourceAccessFlags(*dstResourceUsage, resource, dstBindPoint);
					newDstBarrier.srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
					newDstBarrier.dstStageMask = CalculateResourcePipelineStageFlags(dstBindPoint, newDstBarrier.dstAccessMask, false);
					newDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					newDstBarrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

					pDstRenderPass->m_inputBarriers.insert({ resourceID, newDstBarrier });
					return;
				}
				else if (resource.type == RESOURCE_TYPE::TEXTURE)
				{
					TextureVk* pTexture = ResolveTexture(resource);
					ASSERT_PTR(pTexture);

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

						pDstRenderPass->m_inputBarriers.insert({ resourceID, newDstBarrier });
					}
				}
			});

			for (const DependencyInfo& dependencyInfo : pDstRenderPass->m_dependencyInfos)
			{
				RenderPassVk* pSrcRenderPass = dependencyInfo.renderPass;
				TraverseResources(dependencyInfo.resources, [&](const RenderResource& resourceDependency)
				{
					// Setup the barrier. In this case the source corresponds to the active source render pass we're
					// currently in. The destination corresponds to the dependency we're currently looping through
					const u64& resourceID = resourceDependency.resourceID;
					const ResourceUsage* srcResourceUsage = GetResourceUsageFromPass(*pSrcRenderPass, resourceID);
					ASSERT_PTR(srcResourceUsage); // Should never be null
					const ResourceUsage* dstResourceUsage = GetResourceUsageFromPass(*pDstRenderPass, resourceID);
					ASSERT_PTR(dstResourceUsage); // Should never be null

					const PASS_TYPE srcBindPoint = pSrcRenderPass->m_passType;
					const PASS_TYPE dstBindPoint = pDstRenderPass->m_passType;

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
					pDstRenderPass->m_inputBarriers[resourceID] = newDstBarrier;
					pSrcRenderPass->m_outputBarriers[resourceID] = newDstBarrier;
				});
			}
		}
	}

	bool RenderGraphVk::RequiresExplicitResourceBarrier(const RenderPassVk& renderPass, u64 resourceID) const
	{
		if (renderPass.m_outputBarriers.find(resourceID) != renderPass.m_outputBarriers.end())
		{
			return false;
		}

		return true;
	}

	STATUS_CODE RenderGraphVk::InsertResourceBarriers(const RenderPassVk& renderPass)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;
		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());

		for (auto& barrierIter : renderPass.m_inputBarriers)
		{
			u64 resourceID = barrierIter.first;
			const Barrier& currBarrier = barrierIter.second;

			if (renderPass.m_passType == PASS_TYPE::GRAPHICS)
			{
				// HACK! We prevent barriers from being inserted for output textures that
				// belong to a graphics pass. This is because the render pass implicitly performs
				// these transitions, so we save work and also it's not possible to insert an explicit
				// barrier to transition the backbuffer layout, so we must rely on the render pass implicit
				// transitions anyway. I think ideally this logic would get moved to the CalculateResourceBarriers()
				// function so that the barriers are never created to begin with
				if (!RequiresExplicitResourceBarrier(renderPass, resourceID))
				{
					continue;
				}
			}

			const RenderResource* resourceBarrier = GetPhysicalResource(resourceID);
			ASSERT_PTR(resourceBarrier);

			switch (resourceBarrier->type)
			{
			case RESOURCE_TYPE::BUFFER:
			{
				BufferVk* pBuffer = ResolveBuffer(*resourceBarrier);
				res = pDeviceContext->InsertBufferMemoryBarrier(
					pBuffer,
					ConvertPassTypeToQueueType(renderPass.m_passType),
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
			case RESOURCE_TYPE::ACCELERATION_STRUCTURE:
			{
				AccelerationStructureVk* pAccelerationStructure = ResolveAccelerationStructure(*resourceBarrier);
				res = pDeviceContext->InsertAccelerationStructureMemoryBarrier(
					pAccelerationStructure,
					ConvertPassTypeToQueueType(renderPass.m_passType),
					currBarrier.srcStageMask,
					currBarrier.dstStageMask,
					currBarrier.srcAccessMask,
					currBarrier.dstAccessMask
				);

				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to insert dependency barriers. Could not insert acceleration structure memory barrier!");
					return res;
				}

				break;
			}
			case RESOURCE_TYPE::TEXTURE:
			{
				TextureVk* pTexture = ResolveTexture(*resourceBarrier);
				res = pDeviceContext->InsertImageMemoryBarrier(
					pTexture,
					ConvertPassTypeToQueueType(renderPass.m_passType),
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
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.Size()))
		{
			return;
		}

		const RenderPassVk* pCurrRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		if (callback != nullptr)
		{
			callback(*pCurrRenderPass);
		}

		for (u32 i = 0; i < static_cast<u32>(pCurrRenderPass->m_dependencyInfos.size()); i++)
		{
			const DependencyInfo& dependencyInfo = pCurrRenderPass->m_dependencyInfos[i];
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
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.Size()))
		{
			return;
		}

		const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		TraverseResources(pRenderPass->m_inputResources, callback);
	}

	void RenderGraphVk::TraverseRenderPassOutputs(u32 renderPassIndex, TraverseResourceCallbackFn callback) const
	{
		if (renderPassIndex >= static_cast<u32>(m_registeredRenderPasses.Size()))
		{
			return;
		}

		const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		TraverseResources(pRenderPass->m_outputResources, callback);
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
		RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		for (const auto& iter : pRenderPass->m_outputBarriers)
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

	AccelerationStructureVk* RenderGraphVk::ResolveAccelerationStructure(const RenderResource& resource)
	{
		ASSERT_MSG(resource.type == RESOURCE_TYPE::ACCELERATION_STRUCTURE, "Failed to resolve resource into acceleration structure. Resource is not an acceleration structure type!");

		const AccelerationStructureHandle handle = static_cast<const AccelerationStructureHandle>(resource.handle);
		AccelerationStructureVk* pAccelerationStructure = static_cast<AccelerationStructureVk*>(m_pRenderDevice->ResolveHandle(handle));
		return pAccelerationStructure;
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
		case RESOURCE_TYPE::ACCELERATION_STRUCTURE:
		{
			AccelerationStructureVk* pAccelerationStructure = ResolveAccelerationStructure(resource);
			if (pAccelerationStructure != nullptr)
			{
				return pAccelerationStructure->GetName();
			}
			break;
		}
		case RESOURCE_TYPE::UNIFORM:
		{
			// Unnamed
			break;
		}
		default:
		{
			ASSERT_ALWAYS("Failed to get resource name. Unknown resource type!");
			break;
		}
		}

		return nullptr;
	}

	u64 RenderGraphVk::HashState() const
	{
		size_t seed = 0;

		// Render passes
		HashCombine(seed, m_registeredRenderPasses.Size());
		for (u32 i = 0; i < static_cast<u32>(m_registeredRenderPasses.Size()); i++)
		{
			const RenderPassVk* pCurrRenderPass = m_registeredRenderPasses.Get(i);
			HashCombine(seed, pCurrRenderPass->m_inputResources);
			HashCombine(seed, pCurrRenderPass->m_outputResources);
			// Ignore callbacks
			HashCombine(seed, pCurrRenderPass->m_index);

			HashCombine(seed, pCurrRenderPass->m_passType);
			switch (pCurrRenderPass->m_passType)
			{
			case PASS_TYPE::GRAPHICS:
			{
				GraphicsPipelineDescHasher hasher;
				HashCombine(seed, hasher(pCurrRenderPass->graphicsDesc));
				break;
			}
			case PASS_TYPE::COMPUTE:
			{
				ComputePipelineDescHasher hasher;
				HashCombine(seed, hasher(pCurrRenderPass->computeDesc));
				break;
			}
			case PASS_TYPE::RAY_TRACING:
			{
				RayTracingPipelineDescHasher hasher;
				HashCombine(seed, hasher(pCurrRenderPass->rayTracingDesc));
				break;
			}
			case PASS_TYPE::TRANSFER:
			case PASS_TYPE::AS_BUILD:
			{
				break;
			}
			default:
			{
				ASSERT_ALWAYS("Failed to hash render graph state. Unhandled pass type");
				return 0;
			}
			}

			auto HashBarrierFn = [&](const std::unordered_map<u64, Barrier>& barrierMap, u64& seed)
			{
				HashCombine(seed, barrierMap.size());
				for (const auto& barrierIter : barrierMap)
				{
					// Ignore barrierIter.first (resourceID) - may change per frame
					const Barrier& currInputBarrier = barrierIter.second;
					HashCombine(seed, currInputBarrier.dstAccessMask);
					HashCombine(seed, currInputBarrier.dstStageMask);
					HashCombine(seed, currInputBarrier.srcAccessMask);
					HashCombine(seed, currInputBarrier.srcStageMask);
					HashCombine(seed, currInputBarrier.oldLayout);
					HashCombine(seed, currInputBarrier.newLayout);
				}
			};

			HashBarrierFn(pCurrRenderPass->m_inputBarriers, seed);
			HashBarrierFn(pCurrRenderPass->m_outputBarriers, seed);
		}

		// Resource usages
		HashCombine(seed, m_resourceUsages.size());
		for (u32 i = 0; i < static_cast<u32>(m_resourceUsages.size()); i++)
		{
			const ResourceUsage& currUsage = m_resourceUsages[i];

			HashCombine(seed, currUsage.io);
			HashCombine(seed, currUsage.attachmentType);
			HashCombine(seed, currUsage.storeOp);
			HashCombine(seed, currUsage.loadOp);
			// Ignore clearValue
			HashCombine(seed, currUsage.bufferUsage);
			// Ignore resourceID - derived from handle, may change per frame
			HashCombine(seed, currUsage.passIndex);
		}

		// Physical resources
		HashCombine(seed, m_physicalResources.size());
		for (u32 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			const RenderResource& currResource = m_physicalResources[i];

			// Ignore data that changes per-frame (e.g. swap chain image indices).
			// resourceID is derived from the handle (index + generation) and may
			// change per frame
			HashCombine(seed, currResource.type);
		}

		return seed;
	}

	void RenderGraphVk::CallExecutionCallback(const RenderPassVk& renderPass, const DeviceContextHandle& deviceContext)
	{
		if (renderPass.m_execCallback)
		{
			renderPass.m_execCallback(deviceContext);
			m_didExecuteWork = true;
		}
	}
}