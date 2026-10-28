
#include <algorithm>
#include <vulkan/vk_enum_string_helper.h>

#include "render_graph_vk.h"

#include "acceleration_structure_vk.h"
#include "BSL/logger.h"
#include "BSL/math.h"
#include "BSL/sanity.h"
#include "buffer_vk.h"
#include "core/handle/handle_accessor.h"
#include "core/handle/handle_utils.h"
#include "core/global_settings.h"
#include "core/profiling.h"
#include "device_context_vk.h"
#include "render_device_vk.h"
#include "swap_chain_vk.h"
#include "texture_vk.h"
#include "utils/attachment_type_converter.h"
#include "utils/cache_utils.h"
#include "utils/render_graph_type_converter.h"

// Render graph inspired from:
// https://poniesandlight.co.uk/reflect/island_rendergraph_1/
//

using namespace BSL;

namespace PHX
{
	static constexpr u32 INVALID_RENDER_PASS_INDEX = U32_MAX;

	static VkAccessFlags CalculateResourceAccessFlags(const ResourceUsage& usage, const RenderResource& resource, PASS_TYPE passType)
	{
		const bool isRead  = IsReadAccess(usage.access);
		const bool isWrite = IsWriteAccess(usage.access);

		// Acceleration structures use their own access flags regardless of bind point
		if (resource.type == RESOURCE_TYPE::ACCELERATION_STRUCTURE)
		{
			return (isRead  ? VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR  : 0) |
				   (isWrite ? VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR : 0);
		}

		// Transfer and AS build writes bypass the resource-type breakdown. AS build
		// *reads* still fall through so build-input buffers keep their dedicated flags
		if (passType == PASS_TYPE::TRANSFER || (passType == PASS_TYPE::AS_BUILD && isWrite))
		{
			return (isRead  ? VK_ACCESS_TRANSFER_READ_BIT  : 0) |
				   (isWrite ? VK_ACCESS_TRANSFER_WRITE_BIT : 0);
		}

		VkAccessFlags flags = 0;
		switch (resource.type)
		{
		case RESOURCE_TYPE::BUFFER:
		{
			if (isWrite)
			{
				flags |= VK_ACCESS_SHADER_WRITE_BIT;
			}
			if (isRead)
			{
				const BufferUsageFlags bufferUsage = usage.bufferUsage;
				if (bufferUsage & BUFFER_USAGE_FLAG_VERTEX_BUFFER)
				{
					flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
				}
				if (bufferUsage & BUFFER_USAGE_FLAG_INDEX_BUFFER)
				{
					flags |= VK_ACCESS_INDEX_READ_BIT;
				}
				if (bufferUsage & (BUFFER_USAGE_FLAG_STORAGE_BUFFER | BUFFER_USAGE_FLAG_UNIFORM_BUFFER))
				{
					flags |= VK_ACCESS_SHADER_READ_BIT;
				}
				if (bufferUsage & BUFFER_USAGE_FLAG_INDIRECT_BUFFER)
				{
					flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
				}
				if (bufferUsage & BUFFER_USAGE_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT)
				{
					// These buffers have STORAGE_BUFFER_BIT and may be read as storage buffers in shaders
					flags |= (passType == PASS_TYPE::AS_BUILD) ? VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR : VK_ACCESS_SHADER_READ_BIT;
				}
			}
			break;
		}
		case RESOURCE_TYPE::TEXTURE:
		{
			const bool isAttachment = (usage.attachmentType != ATTACHMENT_TYPE::INVALID) && (passType == PASS_TYPE::GRAPHICS);
			if (!isAttachment)
			{
				flags |= isRead  ? VK_ACCESS_SHADER_READ_BIT  : 0;
				flags |= isWrite ? VK_ACCESS_SHADER_WRITE_BIT : 0;
				break;
			}

			switch (usage.attachmentType)
			{
			case ATTACHMENT_TYPE::COLOR:
			{
				flags |= isRead  ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT  : 0;
				flags |= isWrite ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
				break;
			}
			case ATTACHMENT_TYPE::DEPTH:
			case ATTACHMENT_TYPE::STENCIL:
			case ATTACHMENT_TYPE::DEPTH_STENCIL:
			{
				flags |= isRead  ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT  : 0;
				flags |= isWrite ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
				break;
			}
			case ATTACHMENT_TYPE::RESOLVE:
			{
				// Not sure if this is correct
				flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			}
			default:
			{
				break;
			}
			}
			break;
		}
		case RESOURCE_TYPE::UNIFORM:
		{
			ASSERT_MSG(!isWrite, "Cannot calculate access flag for a written-to uniform!");
			flags |= VK_ACCESS_UNIFORM_READ_BIT;
			break;
		}
		default:
		{
			ASSERT_MSG("Failed to calculate resource access flag. Unknown resource type %u!", static_cast<u32>(resource.type));
			break;
		}
		}

		return flags;
	}

	static VkPipelineStageFlags CalculateResourcePipelineStageFlags(PASS_TYPE passType, VkAccessFlags accessFlags, bool isSrcFlag)
	{
		if (accessFlags == VK_ACCESS_NONE)
		{
			return (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		}

		// Acceleration structure access flags are independent of the pass bind point
		if (accessFlags & (VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR))
		{
			VkPipelineStageFlags flags = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			if ((accessFlags & VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR) && passType == PASS_TYPE::RAY_TRACING)
			{
				flags |= VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			}
			return flags;
		}

		VkPipelineStageFlags flags = 0;
		switch (passType)
		{
		case PASS_TYPE::GRAPHICS:
		{
			if (accessFlags & (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT))
			{
				flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			}
			if (accessFlags & VK_ACCESS_UNIFORM_READ_BIT)
			{
				// TODO - Determine proper shader stage
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			if (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
			{
				flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			if (accessFlags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT))
			{
				// Find a way to tell which shader is writing to the resource so we don't have to include
				// all the shader flags. For now, we're only covering vertex and fragment shaders, but realistically
				// this should cover more shader types like geometry, tesselation, etc but I don't want to
				// guard against all of them here
				TECHDEBT("Read above");
				flags |= (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
			if (accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
			{
				flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
			}
			if (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT))
			{
				// LOAD_OP_LOAD on a color attachment produces a combined READ|WRITE access mask
				// TODO - Might be missing a isSrcFlag check to OR VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			}
			if (accessFlags & (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT))
			{
				// LOAD_OP_LOAD on a depth/stencil attachment produces a combined READ|WRITE access mask
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
			}
			if (accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT))
			{
				ASSERT_ALWAYS("Transfer access flags shouldn't be handled in graphics bind point");
			}
			if (accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT))
			{
				flags |= VK_PIPELINE_STAGE_HOST_BIT;
			}
			if (accessFlags & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT))
			{
				// Not sure if these flags are good, assuming worst-case scenario
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			break;
		}
		case PASS_TYPE::COMPUTE:
		{
			// Compute passes primarily use the compute shader stage, but may also
			// read/write transfer or indirect command resources. Handle each access
			// flag individually to avoid mapping incompatible flags to COMPUTE_SHADER.
			if (accessFlags & (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT))
			{
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			}
			if (accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT))
			{
				flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			if (accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
			{
				flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
			}
			if (accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT))
			{
				flags |= VK_PIPELINE_STAGE_HOST_BIT;
			}
			if (accessFlags & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT))
			{
				flags |= (isSrcFlag ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
			}
			// Fallback: if no specific flag matched, default to compute shader
			if (flags == 0)
			{
				flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			}
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
		switch (usage.access)
		{
		case RESOURCE_ACCESS::READ_ONLY:
		{
			switch (passType)
			{
			case PASS_TYPE::GRAPHICS:
			{
				switch (usage.attachmentType)
				{
				case ATTACHMENT_TYPE::COLOR:
				{
					// LOAD-op attachment read (dual-registered input)
					return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				}
				case ATTACHMENT_TYPE::DEPTH:
				case ATTACHMENT_TYPE::DEPTH_STENCIL:
				case ATTACHMENT_TYPE::STENCIL:
				{
					// LOAD-op attachment read (dual-registered input)
					return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				default:
				{
					// Sampled texture input (SetTextureInput with INVALID)
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
		case RESOURCE_ACCESS::WRITE_ONLY:
		case RESOURCE_ACCESS::READ_WRITE:
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
				// Transfer read-modify-write has no optimal layout covering both directions
				return (usage.access == RESOURCE_ACCESS::READ_WRITE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
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
		m_passType(passType), m_registerResourceCallback(registerResourceCallback), m_index(index), m_isVirtual(false)
	{
		ASSERT_MSG(m_registerResourceCallback != nullptr, "Register resource callback is null");
		m_name = name;
	}

	RenderPassVk::~RenderPassVk()
	{
		m_readResources.reset();
		m_writeResources.reset();
	}

	void RenderPassVk::SetTextureInput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::READ_ONLY;
		usage.passIndex = m_index;
		// Sampled texture input (shader read), not an attachment read. INVALID distinguishes
		// this from the dual-registered INPUT of a LOAD-op attachment, which sets COLOR/DEPTH.
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::IGNORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::LOAD;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_readResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferInput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::READ_ONLY;
		usage.passIndex = m_index;
		usage.bufferUsage = buffer.GetUsage();

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
		m_readResources.set(resourceIndex);
	}

	void RenderPassVk::SetUniformInput(UniformCollectionHandle uniformCollection)
	{
		TODO();
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::READ_ONLY;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(uniformCollection, RESOURCE_TYPE::UNIFORM, usage);
		m_readResources.set(resourceIndex);
	}

	void RenderPassVk::SetAccelerationStructureInput(AccelerationStructureHandle accelerationStructure)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::READ_ONLY;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(accelerationStructure, RESOURCE_TYPE::ACCELERATION_STRUCTURE, usage);
		m_readResources.set(resourceIndex);
	}

	void RenderPassVk::SetColorOutput(TextureHandle texture)
	{
		SetTextureOutput(texture, ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::STORE, {});
	}

	void RenderPassVk::SetDepthOutput(TextureHandle texture)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;
		usage.clearValue.useClearColor = false;
		usage.clearValue.depthStencil.depthClear = 1.0f;
		usage.clearValue.depthStencil.stencilClear = 0;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_writeResources.set(resourceIndex);
	}

	void RenderPassVk::SetDepthStencilOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::DEPTH_STENCIL;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_writeResources.set(resourceIndex);
	}

	void RenderPassVk::SetResolveOutput(TextureHandle texture)
	{
		TODO();
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;
		usage.attachmentType = ATTACHMENT_TYPE::RESOLVE;
		usage.storeOp = ATTACHMENT_STORE_OP::STORE;
		usage.loadOp = ATTACHMENT_LOAD_OP::CLEAR;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);
		m_writeResources.set(resourceIndex);
	}


	void RenderPassVk::SetTextureOutput(TextureHandle texture, ATTACHMENT_LOAD_OP loadOp, ATTACHMENT_STORE_OP storeOp, ClearValues clearValue)
	{
		const ATTACHMENT_TYPE attachmentType = CalculateAttachmentType(texture);

		bool isReadWrite = (loadOp == ATTACHMENT_LOAD_OP::LOAD);

		ResourceUsage usage{};
		usage.access = isReadWrite ? RESOURCE_ACCESS::READ_WRITE : RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;
		usage.attachmentType = attachmentType;
		usage.storeOp = storeOp;
		usage.loadOp = loadOp;
		usage.clearValue = clearValue;

		const ResourceIndex resourceIndex = m_registerResourceCallback(texture, RESOURCE_TYPE::TEXTURE, usage);

		if (isReadWrite)
		{
			m_readResources.set(resourceIndex);
		} // else write-only
		m_writeResources.set(resourceIndex);
	}

	void RenderPassVk::SetBufferOutput(BufferHandle buffer)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;
		usage.bufferUsage = buffer.GetUsage();

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(buffer, RESOURCE_TYPE::BUFFER, usage);
		m_writeResources.set(resourceIndex);
	}

	void RenderPassVk::SetAccelerationStructureOutput(AccelerationStructureHandle accelerationStructure)
	{
		ResourceUsage usage{};
		usage.access = RESOURCE_ACCESS::WRITE_ONLY;
		usage.passIndex = m_index;

		// Not a texture resource
		usage.attachmentType = ATTACHMENT_TYPE::INVALID;
		usage.storeOp = ATTACHMENT_STORE_OP::INVALID;
		usage.loadOp = ATTACHMENT_LOAD_OP::INVALID;

		const ResourceIndex resourceIndex = m_registerResourceCallback(accelerationStructure, RESOURCE_TYPE::ACCELERATION_STRUCTURE, usage);
		m_writeResources.set(resourceIndex);
	}

	void RenderPassVk::SetPipelineDescription(const GraphicsPipelineDesc& graphicsPipelineDesc)
	{
		if (m_passType != PASS_TYPE::GRAPHICS)
		{
			LogError("Attempting to bind a non-graphics pipeline in a graphics render pass \"%s\"", m_name);
		}

		m_pipelineDesc = graphicsPipelineDesc;
	}

	void RenderPassVk::SetPipelineDescription(const ComputePipelineDesc& computePipelineDesc)
	{
		if (m_passType != PASS_TYPE::COMPUTE)
		{
			LogError("Attempting to bind a non-compute pipeline in a compute render pass \"%s\"", m_name);
		}

		m_pipelineDesc = computePipelineDesc;
	}

	void RenderPassVk::SetPipelineDescription(const RayTracingPipelineDesc& rayTracingPipelineDesc)
	{
		if (m_passType != PASS_TYPE::RAY_TRACING)
		{
			LogError("Attempting to bind a non-raytracing pipeline in a raytracing render pass \"%s\"", m_name);
		}

		m_pipelineDesc = rayTracingPipelineDesc;
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
		m_frameInFlightIndex(0), m_frameNumber(0), m_presentResID(0), m_metrics()
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
	}

	RenderGraphVk::~RenderGraphVk()
	{
		m_deviceContextHandles.clear();
		m_registeredRenderPasses.DeleteAll();
	}

	STATUS_CODE RenderGraphVk::BeginFrame(SwapChainHandle swapChain)
	{
		PROFILE_SCOPE("RenderGraphVk_BeginFrame");

		if (!swapChain.IsValid())
		{
			LogError("Failed to begin frame. Swap chain handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(m_pRenderDevice->ResolveHandle(swapChain));
		ASSERT_PTR(swapChainVk);

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());
		ASSERT_PTR(pDeviceContext);

		// Metrics
		if (GetSettings().gatherMetrics)
		{
			// Reset to defaults
			m_metrics = Metrics{};
			pDeviceContext->SetMetricsPointer(&m_metrics);
		}

		res = pDeviceContext->BeginFrame(swapChainVk);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to begin frame. Device context could not begin frame!");
			return res;
		}

		// Re-register the virtual input pass now so it's always "first in submission order"
		RegisterVirtualPasses();

		return res;
	}

	STATUS_CODE RenderGraphVk::EndFrame(SwapChainHandle swapChain)
	{
		PROFILE_SCOPE("RenderGraphVk_EndFrame");

		if (!swapChain.IsValid())
		{
			LogError("Failed to begin frame. Swap chain handle is invalid!");
			return STATUS_CODE::ERR_API;
		}

		SwapChainVk* swapChainVk = static_cast<SwapChainVk*>(m_pRenderDevice->ResolveHandle(swapChain));
		ASSERT_PTR(swapChainVk);

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());

		res = pDeviceContext->EndFrame(swapChainVk);
		if (res != STATUS_CODE::SUCCESS)
		{
			LogError("Failed to end frame #%u. Device context could not flush!", m_frameNumber);
		}

		// Present
		if (res == STATUS_CODE::SUCCESS && pDeviceContext->WasWorkFlushed())
		{
			res = swapChainVk->Present();
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to end frame #%u. Swap chain present failed!", m_frameNumber);
			}
		}

		// Now that all the work has been done for the current frame, move onto the next one
		m_frameInFlightIndex = (m_frameInFlightIndex + 1) % m_pRenderDevice->GetFramesInFlight();
		m_frameNumber++;

		m_registeredRenderPasses.DeleteAll();

		for (auto& it : m_resourcesUsageChain)
		{
			it.second.clear();
		}
		m_resourcesUsageChain.clear();

		m_physicalResources.clear();

		PROFILE_LOOP("Frame");

		return res;
	}

	STATUS_CODE RenderGraphVk::RegisterPass(const char* passName, PASS_TYPE passType, RenderPassHandle& renderPass)
	{
		RenderPassVk* pRenderPass = RegisterPass_Internal(passName, passType);
		ASSERT_PTR(pRenderPass);

		// NOTE - Manually call PopulateHandle() from HandleAccessor vs using HANDLE_UTILS, 
		// since that inserts an InterfaceT pointer into an array
		HandleAccessor::PopulateHandle(renderPass, this, pRenderPass->m_index);
		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE RenderGraphVk::Bake(SwapChainHandle swapChain)
	{
		PROFILE_SCOPE("RenderGraphVk_Bake");

		STATUS_CODE res = STATUS_CODE::SUCCESS;

		// Resolve the current swapchain image and compute its resource ID.
		// All passes writing the swapchain target this same handle for the current frame.
		TextureHandle currentImage = swapChain.GetCurrentImage();
		m_presentResID = HashResource(currentImage, RESOURCE_TYPE::TEXTURE);

		u32 presentRenderPassIndex = FindPresentRenderPassIndex(m_presentResID);

		// Create the render graph tree using the following steps:

		// 1. Build the "virtual" pass that simulates the work done in the previous frame as a single pass
		const u32 inputPassIndex = 0; // This is assumed to ALWAYS be true, and asserted in RegisterVirtualInputPass()
		BuildVirtualInputPass(inputPassIndex);
		
		// 2. Once that render pass is found, build the dependency tree
		BuildDependencyTree(presentRenderPassIndex);

		// 3. [TRIMMING] Accumulate all contributing render passes into a separate container for the render graph. This is done so that
		//               all non-contributing passes are indirectly trimmed
		std::vector<u32> activePassIndices;
		CalculateActivePasses(presentRenderPassIndex, activePassIndices);

		// 4. Removes all resource usages from trimmed passes
		AdjustResourceUsages(activePassIndices);

		// 4. [COMBINATION] Combine as many separate render passes into one for optimal GPU usage
		// TODO

		// Reverse the order of the active passes so we submit work starting from the earlier submission order
		// NOTE - This is not the same as simply iterating over registered passes in order, since this list
		//        of active passes is built from the dependency tree, so the order may not always align
		//        with the exact submission order. This is the more correct way to submit work!
		std::reverse(activePassIndices.begin(), activePassIndices.end());

		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());
		DeviceContextHandle deviceContext = GetCurrentDeviceContextHandle();

		// Traverse the passes in submission order, skipping invalid passes (virtual, inactive, etc)
		for (u32 passIndex : activePassIndices)
		{
			RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(passIndex);
			ASSERT_PTR(pRenderPass);

			RenderPassVk& currRenderPass = *pRenderPass;
			if (currRenderPass.m_isVirtual)
			{
				continue;
			}

			// Before calling execution callback, calculate all barriers necessary for the pass
			res = CalculateResourceBarriers(currRenderPass);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to bake render graph. Could not calculate resource barriers!");
				return res;
			}

			res = InsertResourceBarriers(currRenderPass);
			if (res != STATUS_CODE::SUCCESS)
			{
				LogError("Failed to bake render graph. Could not insert resource barriers!");
				return res;
			}

			// Insert a label for GPU operations
			{
				const QUEUE_TYPE passQueueType = ConvertPassTypeToQueueType(currRenderPass.m_passType);
				pDeviceContext->BeginLabel(passQueueType, currRenderPass.m_name);
			}

			switch (currRenderPass.m_passType)
			{
			case PASS_TYPE::GRAPHICS:
			{
				// Get or create render pass (refers to internal cache)
				VkRenderPass renderPassVk = CreateRenderPass(currRenderPass);

				WriteBeginTimestamp(pDeviceContext, currRenderPass);

				// Get or create framebuffer from render device (refers to internal cache)
				// isBackbuffer: true if this pass writes the swapchain image (triggers resize invalidation)
				const ResourceUsage* backbufferUsage = GetResourceUsageFromPass(m_presentResID, currRenderPass.m_index, 0);
				const bool isBackbuffer = (backbufferUsage != nullptr) ? IsWriteAccess(backbufferUsage->access) : false;
				FramebufferVk* pFramebuffer = CreateFramebuffer(currRenderPass, renderPassVk, isBackbuffer);

				// Build per-attachment clear values from each output's ResourceUsage.clearValue
				std::vector<ClearValues> clearValues;
				TraverseResources(currRenderPass.m_writeResources, [&](const RenderResource& resource)
				{
					if (resource.type != RESOURCE_TYPE::TEXTURE)
					{
						return;
					}

					const ResourceUsage* currUsage = GetResourceUsageFromPass(resource.resourceID, currRenderPass.m_index, 0);
					if (currUsage != nullptr)
					{
						clearValues.push_back(currUsage->clearValue);
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
				const GraphicsPipelineDesc& graphicsPipelineDesc = std::get<GraphicsPipelineDesc>(currRenderPass.m_pipelineDesc);
				const bool hasPipeline = (graphicsPipelineDesc.shaderCount > 0 && graphicsPipelineDesc.pShaders != nullptr);

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

				WriteEndTimestamp(pDeviceContext);

				res = pDeviceContext->EndRenderPass();
				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to bake render graph. Device context could not end render pass!");
					return res;
				}

				break;
			}
			case PASS_TYPE::COMPUTE:
			{
				// Get or create pipeline from render device (refers to internal cache)
				// NOTE - The render pass isn't used for compute pipeline creation, so it can
				// be ignored by passing in VK_NULL_HANDLE
				PipelineVk* pPipeline = CreatePipeline(currRenderPass, VK_NULL_HANDLE);

				WriteBeginTimestamp(pDeviceContext, currRenderPass);

				pDeviceContext->SetContextualPipeline(pPipeline);
				CallExecutionCallback(currRenderPass, deviceContext);
				pDeviceContext->ResetContextualPipeline();

				WriteEndTimestamp(pDeviceContext);

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
			m_metrics.passCount = static_cast<u32>(activePassIndices.size());
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
		if (m_registeredRenderPasses.Empty())
		{
			LogError("Failed to generate render graph visualization. No render passes registered - call after Bake()!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		if (generateIfUnique && m_uniqueVisualizationHashes.find(m_currentFrameGraphHash) != m_uniqueVisualizationHashes.end())
		{
			// We've already generated a visualization for the current state. Don't re-generate
			return STATUS_CODE::SUCCESS;
		}

		// Build a backend-agnostic snapshot of the baked render graph
		const u32 passCount = m_registeredRenderPasses.Size();

		VizGraph graph{};
		graph.finalPassIndex = FindPresentRenderPassIndex(m_presentResID);
		ASSERT(graph.finalPassIndex != INVALID_RENDER_PASS_INDEX);

		// Executed passes in submission order (same procedure as Bake())
		CalculateActivePasses(graph.finalPassIndex, graph.submissionOrder);
		std::reverse(graph.submissionOrder.begin(), graph.submissionOrder.end());
		graph.submissionOrder.erase(std::remove_if(graph.submissionOrder.begin(), graph.submissionOrder.end(), [&](u32 passIndex)
		{
			const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(passIndex);
			return (pRenderPass == nullptr || pRenderPass->m_isVirtual);
		}), graph.submissionOrder.end());

		graph.passes.resize(passCount);
		for (u32 passIndex = 0; passIndex < passCount; passIndex++)
		{
			const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(passIndex);
			if (pRenderPass != nullptr)
			{
				graph.passes[passIndex] = { pRenderPass->m_name, pRenderPass->m_passType, pRenderPass->m_isVirtual };
			}
		}

		graph.resources.reserve(m_physicalResources.size());
		for (const RenderResource& physicalResource : m_physicalResources)
		{
			VizResource vizResource{};
			const char* resourceName = GetResourceName(physicalResource);
			vizResource.name = (resourceName != nullptr) ? resourceName : "";
			vizResource.type = physicalResource.type;
			vizResource.isPresent = (physicalResource.resourceID == m_presentResID);

			auto chainIter = m_resourcesUsageChain.find(physicalResource.resourceID);
			if (chainIter != m_resourcesUsageChain.end())
			{
				for (const ResourceUsage& usage : chainIter->second)
				{
					VizUsage vizUsage{};
					vizUsage.passIndex = usage.passIndex;
					vizUsage.access = usage.access;
					vizUsage.attachmentType = usage.attachmentType;
					vizUsage.loadOp = usage.loadOp;
					vizUsage.storeOp = usage.storeOp;
					vizUsage.bufferUsage = usage.bufferUsage;

					// Use the barrier that was actually calculated for this pass during Bake()
					const RenderPassVk* pRenderPass = m_registeredRenderPasses.Get(usage.passIndex);
					auto barrierIter = pRenderPass->m_barriers.find(physicalResource.resourceID);
					if (barrierIter != pRenderPass->m_barriers.end())
					{
						const Barrier& barrier = barrierIter->second;
						if (physicalResource.type == RESOURCE_TYPE::TEXTURE)
						{
							vizUsage.layout = RG_UTILS::ShortImageLayout(barrier.currentLayout);
							vizUsage.barrierLines.push_back("layout: " + RG_UTILS::ShortImageLayout(barrier.oldLayout) + " -> " + vizUsage.layout);
							if (barrier.nextLayout != barrier.currentLayout)
							{
								vizUsage.barrierLines.push_back("final layout: " + RG_UTILS::ShortImageLayout(barrier.nextLayout));
							}
						}
						vizUsage.barrierLines.push_back("stage: " + string_VkPipelineStageFlags(barrier.srcStageMask) + " -> " + string_VkPipelineStageFlags(barrier.dstStageMask));
						vizUsage.barrierLines.push_back("access: " + string_VkAccessFlags(barrier.srcAccessMask) + " -> " + string_VkAccessFlags(barrier.dstAccessMask));
					}

					vizResource.usages.push_back(std::move(vizUsage));
				}

				// The virtual input pass registers its usages during Bake(), which places them at the end of the chain
				std::stable_sort(vizResource.usages.begin(), vizResource.usages.end(), [](const VizUsage& a, const VizUsage& b)
				{
					return a.passIndex < b.passIndex;
				});
			}

			graph.resources.push_back(std::move(vizResource));
		}

		const STATUS_CODE res = CreateGraphVisualization(graph, fileName);
		if (res == STATUS_CODE::SUCCESS)
		{
			// Cache the hash that we've generated a visualization for
			m_uniqueVisualizationHashes.insert({ m_currentFrameGraphHash, true });
		}

		return res;
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
		PROFILE_SCOPE("RenderGraphVk_ResolveHandle");

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
		PROFILE_SCOPE("RenderGraphVk_CreateRenderPass");

		RenderPassDescription renderPassDesc{};
		renderPassDesc.attachments.reserve(renderPass.m_writeResources.size());

		TECHDEBT("Support multiple subpasses");
		renderPassDesc.subpasses.reserve(1);

		SubpassDescription subpassDesc{};
		subpassDesc.bindPoint = RG_UTILS::ConvertPassTypeToBindPoint(renderPass.m_passType);

		// Incremented every iteration of TraverseRenderPassOutputs below
		u32 localResourceIndex = 0;

		TraverseResources(renderPass.m_writeResources, [&](const RenderResource& physicalResource)
		{
			if (physicalResource.type != RESOURCE_TYPE::TEXTURE)
			{
				// Ignore any non-texture output resources
				return;
			}

			TextureVk* pTexture = ResolveTexture(physicalResource);
			ASSERT_PTR(pTexture);

			AttachmentDescription attDesc{};
			attDesc.pTexture = pTexture;

			{
				ResourceID resourceID = physicalResource.resourceID;
				auto iter = renderPass.m_barriers.find(resourceID);
				if (iter == renderPass.m_barriers.end())
				{
					ASSERT_ALWAYS("Failed to create render pass. Could not find barrier for resource");
				}
				const Barrier& barrier = iter->second;

				// These layout variables must know about the next usage, so we use the currToNext barrier for these
				attDesc.initialLayout = barrier.oldLayout;
				attDesc.layout = barrier.currentLayout;
				attDesc.finalLayout = barrier.nextLayout;

				// These masks guard the subpass operations against any previous work, so we use the prevToCurr barrier for these
				subpassDesc.srcAccessMask |= barrier.srcAccessMask;
				subpassDesc.dstAccessMask |= barrier.dstAccessMask;
				subpassDesc.srcStageMask  |= barrier.srcStageMask;
				subpassDesc.dstStageMask  |= barrier.dstStageMask;

				// Adjust the texture layout so it reflects the automatic render pass layout transitions
				pTexture->SetLayout(barrier.nextLayout);

				const ResourceUsage* currUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, 0);
				switch (currUsage->attachmentType)
				{
				case ATTACHMENT_TYPE::COLOR:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(currUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(currUsage->storeOp);

					subpassDesc.colorAttachmentIndices.push_back(localResourceIndex);
					break;
				}
				case ATTACHMENT_TYPE::DEPTH:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(currUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(currUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::STENCIL:
				{
					attDesc.stencilLoadOp = ATT_UTILS::ConvertLoadOp(currUsage->loadOp);
					attDesc.stencilStoreOp = ATT_UTILS::ConvertStoreOp(currUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::DEPTH_STENCIL:
				{
					// TODO - Should this be considered a stencil or regular load/store op?
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(currUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(currUsage->storeOp);

					ASSERT_MSG(subpassDesc.depthStencilAttachmentIndex == -1, "Already assigned the depth stencil attachment index!");
					subpassDesc.depthStencilAttachmentIndex = localResourceIndex;
					break;
				}
				case ATTACHMENT_TYPE::RESOLVE:
				{
					attDesc.loadOp = ATT_UTILS::ConvertLoadOp(currUsage->loadOp);
					attDesc.storeOp = ATT_UTILS::ConvertStoreOp(currUsage->storeOp);

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
		PROFILE_SCOPE("RenderGraphVk_CreateFramebuffer");

		std::vector<FramebufferAttachmentDesc> attachments;
		attachments.reserve(renderPass.m_writeResources.size());

		u32 maxWidth = 0;
		u32 maxHeight = 0;
		TraverseResources(renderPass.m_writeResources, [&](const RenderResource& outputResource)
		{
			if (outputResource.type != RESOURCE_TYPE::TEXTURE)
			{
				return;
			}

			TextureVk* pAttachmentTex = ResolveTexture(outputResource);
			if (pAttachmentTex == nullptr)
			{
				LogError("Failed to create framebuffer for render pass \"%s\"! Output texture resource does not have a valid texture pointer", renderPass.m_name);
				return;
			}

			const ResourceUsage* resourceUsage = GetResourceUsageFromPass(outputResource.resourceID, renderPass.m_index, 0);
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
		FramebufferVk* pFramebuffer = m_pRenderDevice->GetOrCreateFramebuffer(framebufferCI);
		return pFramebuffer;
	}

	PipelineVk* RenderGraphVk::CreatePipeline(const RenderPassVk& renderPass, VkRenderPass renderPassVk)
	{
		PROFILE_SCOPE("RenderGraphVk_CreatePipeline");

		PipelineVk* pipeline = nullptr;

		PASS_TYPE renderPassBindPoint = renderPass.m_passType;
		switch (renderPassBindPoint)
		{
		case PASS_TYPE::GRAPHICS:
		{
			const GraphicsPipelineDesc& pipelineDesc = std::get<GraphicsPipelineDesc>(renderPass.m_pipelineDesc);
			pipeline = m_pRenderDevice->GetOrCreateGraphicsPipeline(pipelineDesc, renderPassVk);
			break;
		}
		case PASS_TYPE::COMPUTE:
		{
			const ComputePipelineDesc& pipelineDesc = std::get<ComputePipelineDesc>(renderPass.m_pipelineDesc);
			pipeline = m_pRenderDevice->GetOrCreateComputePipeline(pipelineDesc);
			break;
		}
		case PASS_TYPE::RAY_TRACING:
		{
			const RayTracingPipelineDesc& pipelineDesc = std::get<RayTracingPipelineDesc>(renderPass.m_pipelineDesc);
			pipeline = m_pRenderDevice->GetOrCreateRayTracingPipeline(pipelineDesc);
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
			RenderResource& physicalResource = m_physicalResources[i];
			if (physicalResource.resourceID == resourceID)
			{
				// Found match
				physicalResourceIndex = i;
				break;
			}
		}
		
		// Couldn't find existing physical resource, create one instead.
		if (physicalResourceIndex == MAX_REGISTERED_RESOURCES)
		{
			TECHDEBT("Defer physical resource creation until baking?");
			RenderResource newPhysicalResource{};
			newPhysicalResource.resourceID = resourceID;
			newPhysicalResource.handle = resource;
			newPhysicalResource.type = type;

			physicalResourceIndex = numPhysicalResources;
			m_physicalResources.push_back(newPhysicalResource);

			// Create a new resource usage chain as well
			m_resourcesUsageChain[resourceID] = {};
		}

		// Create logical resource
		ResourceUsage newUsage = usage;
		newUsage.resourceID = resourceID;

		// Insert new ResourceUsage into the chain
		auto& resourceUsageChain = m_resourcesUsageChain[resourceID];
		resourceUsageChain.push_back(newUsage);

		return physicalResourceIndex;
	}

	RenderPassVk* RenderGraphVk::RegisterPass_Internal(const char* passName, PASS_TYPE passType)
	{
		auto registerResourceFuncPtr = std::bind(&RenderGraphVk::RegisterResource, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

		RenderPassVk* newRenderPass = new RenderPassVk(passName, passType, 0u, registerResourceFuncPtr);
		const u32 passIndex = m_registeredRenderPasses.Allocate(newRenderPass);
		newRenderPass->m_index = passIndex;

		return newRenderPass;
	}

	u32 RenderGraphVk::FindPresentRenderPassIndex(ResourceID presentResID)
	{
		PROFILE_SCOPE("RenderGraphVk_FindPresentRenderPassIndex");

		auto iter = m_resourcesUsageChain.find(presentResID);
		if (iter == m_resourcesUsageChain.end())
		{
			LogError("Failed to find present render pass index. Resource usage chain is not registered to resource ID %u", presentResID);
			return INVALID_RENDER_PASS_INDEX;
		}

		u32 presentRenderPassIndex = 0;
		auto& resourceUsageChain = iter->second;
		for (const ResourceUsage& usage : resourceUsageChain)
		{
			if (!IsWriteAccess(usage.access))
			{
				continue;
			}

			// Track the last (highest passIndex) writer - it owns presentation
			presentRenderPassIndex = BSL::Max(usage.passIndex, presentRenderPassIndex);
		}

		return presentRenderPassIndex;
	}

	void RenderGraphVk::RegisterVirtualPasses()
	{
		PROFILE_SCOPE("RenderGraphVk_RegisterVirtualPasses");

		RenderPassVk* pInputPass = RegisterPass_Internal("VirtualInputPass", PASS_TYPE::GRAPHICS);
		ASSERT_PTR(pInputPass);
		pInputPass->m_isVirtual = true;

		ASSERT_MSG(pInputPass->m_index == 0, "Virtual input pass is not the first registered pass??");
	}

	void RenderGraphVk::BuildVirtualInputPass(u32 inputPassIndex)
	{
		PROFILE_SCOPE("RenderGraphVk_BuildVirtualInputPass");

		RenderPassVk* pInputPass = m_registeredRenderPasses.Get(inputPassIndex);
		ASSERT_PTR(pInputPass); // We should never fail to get this unless the index is wrong!

		// Loop through all resources and register input-only and output-only resources
		for (const RenderResource& physicalResource : m_physicalResources)
		{
			TECHDEBT("Ugly resource registration. Clean up");
			// Virtual input pass writes are declared for resources whose first usage is read-only
			const auto& resourceUsageChain = m_resourcesUsageChain[physicalResource.resourceID];
			if (resourceUsageChain.size() <= 0)
			{
				continue;
			}

			const ResourceUsage& firstUsage = resourceUsageChain[0];
			if (firstUsage.access == RESOURCE_ACCESS::READ_ONLY)
			{
				switch (physicalResource.type)
				{
				case RESOURCE_TYPE::TEXTURE:
				{
					pInputPass->SetTextureOutput(static_cast<TextureHandle>(physicalResource.handle), ATTACHMENT_LOAD_OP::IGNORE, ATTACHMENT_STORE_OP::IGNORE);
					break;
				}
				case RESOURCE_TYPE::BUFFER:
				{
					pInputPass->SetBufferOutput(static_cast<BufferHandle>(physicalResource.handle));
					break;
				}
				default:
				{
					ASSERT_ALWAYS("Failed to register virtual passes. Unhandled resource type!");
					break;
				}
				}
			}
		}
	}

	void RenderGraphVk::BuildDependencyTree(u32 renderPassIndex)
	{
		PROFILE_SCOPE("RenderGraphVk_BuildDependencyTree");

		// Base cases
		if (renderPassIndex >= m_registeredRenderPasses.Size())
		{
			return;
		}

		RenderPassVk* pCurrRenderPass = m_registeredRenderPasses.Get(renderPassIndex);
		if (pCurrRenderPass->m_readResources.none() && pCurrRenderPass->m_writeResources.none())
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

			const ResourceIndexBitset rawHazardResources = (pPrevRenderPass->m_writeResources & pCurrRenderPass->m_readResources);
			const ResourceIndexBitset warHazardResources = (pPrevRenderPass->m_readResources  & pCurrRenderPass->m_writeResources);
			const ResourceIndexBitset wawHazardResources = (pPrevRenderPass->m_writeResources & pCurrRenderPass->m_writeResources);

			// Add both dependency directions
			const ResourceIndexBitset hazardResources = (rawHazardResources | warHazardResources | wawHazardResources);
			const bool isDependencyRP = hazardResources.any();
			if (isDependencyRP)
			{
				auto InsertDependency = [&](const DependencyInfo& dep, std::vector<DependencyInfo>& depList)
				{
					DependencyInfo* pExistingDependency = nullptr;
					for (auto& currDep : depList)
					{
						if (currDep.renderPass->m_index == dep.renderPass->m_index)
						{
							pExistingDependency = &currDep;
							break;
						}
					}

					if (pExistingDependency == nullptr)
					{
						// No match found, insert new entry
						depList.push_back(dep);
					}
					else
					{
						// If it already exists, combine resource dependency mask
						pExistingDependency->resources |= dep.resources;
					}
				};

				DependencyInfo newDependency{};

				// Forward-facing dependency
				{
					newDependency.renderPass = pCurrRenderPass;
					newDependency.resources = hazardResources;
					InsertDependency(newDependency, pPrevRenderPass->m_forwardDependencies);
				}

				// Backward-facing dependency
				{
					newDependency.renderPass = pPrevRenderPass;
					newDependency.resources = hazardResources;
					InsertDependency(newDependency, pCurrRenderPass->m_backwardDependencies);
				}

				BuildDependencyTree(i);
			}
		}
	}

	void RenderGraphVk::CalculateActivePasses(u32 presentRenderPassIndex, std::vector<u32>& out_activePassIndices)
	{
		PROFILE_SCOPE("RenderGraphVk_CalculateActivePasses");

		const u32 passCount = static_cast<u32>(m_registeredRenderPasses.Size());

		TraverseDependencyTree(presentRenderPassIndex, TraversalDirection::BACKWARD, [&](const RenderPassVk& currRenderPass)
		{
			const u32 passIndex = currRenderPass.m_index;
			if (passIndex < passCount)
			{
				auto lastPassIndexIter = std::find(out_activePassIndices.begin(), out_activePassIndices.end(), passIndex);
				const bool alreadyActive = (lastPassIndexIter != out_activePassIndices.end());
				if (alreadyActive)
				{
					// This means the same pass appeared as a dependency for an earlier pass
					// in submission order. Therefore we must push it to the back of the list
					std::rotate(lastPassIndexIter, lastPassIndexIter + 1, out_activePassIndices.end());
				}
				else
				{
					// Push back new active pass
					out_activePassIndices.push_back(passIndex);
				}
			}
		});
	}

	void RenderGraphVk::AdjustResourceUsages(const std::vector<u32>& activePassIndices)
	{
		PROFILE_SCOPE("RenderGraphVk_AdjustResourceUsages");

		// Build LUT
		std::unordered_map<u32, bool> activePassLUT;
		for (u32 activePassIndex : activePassIndices)
		{
			activePassLUT[activePassIndex] = true;
		}

		auto IsPassTrimmed = [&](u32 passIndex)
		{
			auto iter = activePassLUT.find(passIndex);
			return (iter == activePassLUT.end());
		};

		// Remove entries from each individual chain
		for (auto iter = m_resourcesUsageChain.begin(); iter != m_resourcesUsageChain.end();)
		{
			std::vector<ResourceUsage>& resourceUsageChain = iter->second;
			resourceUsageChain.erase(std::remove_if(resourceUsageChain.begin(), resourceUsageChain.end(),
				[&](const ResourceUsage& usage)
				{
					return IsPassTrimmed(usage.passIndex);
				}),
				resourceUsageChain.end());

			// Remove entire chains if they're empty
			if (resourceUsageChain.empty())
			{
				iter = m_resourcesUsageChain.erase(iter);
				continue;
			}

			iter++;
		}
	}

	STATUS_CODE RenderGraphVk::CalculateResourceBarriers(RenderPassVk& renderPass)
	{
		PROFILE_SCOPE("RenderGraphVk_CalculateResourceBarriers");

		auto AddBarrier = [&](const RenderResource& physicalResource, Barrier& barrier)
		{
			auto iter = renderPass.m_barriers.find(physicalResource.resourceID);
			if (iter != renderPass.m_barriers.end())
			{
				// Combine with existing barrier. Only the layout calculations produce
				// barriers with layout info. Since they happen at the end, simply replace
				// any existing layout data with the incoming barrier's layout data
				Barrier& newBarrier = iter->second;
				newBarrier.srcAccessMask |= barrier.srcAccessMask;
				newBarrier.dstAccessMask |= barrier.dstAccessMask;
				newBarrier.srcStageMask |= barrier.srcStageMask;
				newBarrier.dstStageMask |= barrier.dstStageMask;
				newBarrier.oldLayout = barrier.oldLayout;
				newBarrier.currentLayout = barrier.currentLayout;
				newBarrier.nextLayout = barrier.nextLayout;
			}
			else
			{
				// No existing barrier, just add a new entry
				barrier.physicalResource = physicalResource;
				renderPass.m_barriers[physicalResource.resourceID] = barrier;
			}
		};

		// Calculate sync barriers by looping through backward dependencies
		for (const DependencyInfo& dependency : renderPass.m_backwardDependencies)
		{
			// Only include sync information (access/pipeline masks) when the current pass and it's
			// dependency are on the same queue. Otherwise, the syncs are handled automatically
			// by the semaphore that syncs cross-queue submissions (see DeviceContextVk::EndFrame)
			QUEUE_TYPE currPassQueue = ConvertPassTypeToQueueType(renderPass.m_passType);
			QUEUE_TYPE depPassQueue = ConvertPassTypeToQueueType(dependency.renderPass->m_passType);

			if (currPassQueue != depPassQueue)
			{
				continue;
			}

			TraverseResources(dependency.resources, [&](const RenderResource& physicalResource)
			{
				const ResourceUsage* currUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, 0);
				ASSERT_PTR(currUsage);

				const ResourceUsage* depUsage = GetResourceUsageFromPass(physicalResource.resourceID, dependency.renderPass->m_index, 0);
				ASSERT_PTR(depUsage);

				Barrier barrier = BuildSyncResourceBarrier(physicalResource, *depUsage, *currUsage);
				if (barrier.IsValid())
				{
					AddBarrier(physicalResource, barrier);
				}
			});
		}

		// Calculate layout barriers for all READ and/or WRITE resources
		TraverseResources(renderPass.m_writeResources | renderPass.m_readResources, [&](const RenderResource& physicalResource)
		{
			if (physicalResource.type != RESOURCE_TYPE::TEXTURE)
			{
				return;
			}

			const ResourceUsage* currUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, 0);
			ASSERT_PTR(currUsage);

			// May be null
			const ResourceUsage* prevUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, -1);
			const ResourceUsage* nextUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, 1);

			Barrier barrier = BuildLayoutResourceBarrier(physicalResource, prevUsage, currUsage, nextUsage);
			if (barrier.IsValid())
			{
				AddBarrier(physicalResource, barrier);
			}
		});

		return STATUS_CODE::SUCCESS;
	}

	Barrier RenderGraphVk::BuildSyncResourceBarrier(const RenderResource& physicalResource, const ResourceUsage& srcUsage, const ResourceUsage& dstUsage)
	{
		// The barriers must solve two separate issues:
		// 1. They must prevent hazards (RAW, WAR, WAW)
		// 2. In the case of textures, they must transition the layout to the correct one

		const RenderPassVk* pSrcPass = m_registeredRenderPasses.Get(srcUsage.passIndex);
		ASSERT_PTR(pSrcPass);
		const PASS_TYPE srcPassType = pSrcPass->m_passType;

		const RenderPassVk* pDstPass = m_registeredRenderPasses.Get(dstUsage.passIndex);
		ASSERT_PTR(pDstPass);
		const PASS_TYPE dstPassType = pDstPass->m_passType;

		Barrier barrier(physicalResource);

		VkAccessFlags srcAccessFlags = VK_ACCESS_NONE;
		VkAccessFlags dstAccessFlags = VK_ACCESS_NONE;
		bool isSync = false;
		if (!pDstPass->m_isVirtual)
		{
			srcAccessFlags = CalculateResourceAccessFlags(srcUsage, physicalResource, srcPassType);
			dstAccessFlags = CalculateResourceAccessFlags(dstUsage, physicalResource, dstPassType);

			barrier.srcAccessMask = srcAccessFlags;
			barrier.dstAccessMask = dstAccessFlags;
			isSync = true;
		}

		// Only insert stage masks if we have sync data, otherwise Barrier::IsValid() will fail
		if (isSync)
		{
			barrier.srcStageMask = CalculateResourcePipelineStageFlags(srcPassType, srcAccessFlags, true);
			barrier.dstStageMask = CalculateResourcePipelineStageFlags(dstPassType, dstAccessFlags, false);
		}

		return barrier;
	}

	Barrier RenderGraphVk::BuildLayoutResourceBarrier(const RenderResource& physicalResource, const ResourceUsage* prevUsage, const ResourceUsage* currUsage, const ResourceUsage* nextUsage)
	{
		if (physicalResource.type != RESOURCE_TYPE::TEXTURE)
		{
			// No work to do
			return Barrier(physicalResource);
		}

		if (currUsage == nullptr)
		{
			// currUsage may not be null!
			LogError("Failed to calculate layout resource barrier. Current usage is null!");
			return Barrier(physicalResource);
		}

		// May be null
		PASS_TYPE prevPassType = PASS_TYPE::GRAPHICS; // No good default
		if (prevUsage != nullptr)
		{
			const RenderPassVk* pPrevPass = m_registeredRenderPasses.Get(prevUsage->passIndex);
			ASSERT_PTR(pPrevPass);
			prevPassType = pPrevPass->m_passType;
		}

		const RenderPassVk* pCurrPass = m_registeredRenderPasses.Get(currUsage->passIndex);
		ASSERT_PTR(pCurrPass);
		const PASS_TYPE currPassType = pCurrPass->m_passType;

		// May be null
		PASS_TYPE nextPassType = PASS_TYPE::GRAPHICS; // No good default
		if (nextUsage != nullptr)
		{
			const RenderPassVk* pNextPass = m_registeredRenderPasses.Get(nextUsage->passIndex);
			ASSERT_PTR(pNextPass);
			nextPassType = pNextPass->m_passType;
		}

		Barrier barrier(physicalResource);

		TextureVk* textureResource = ResolveTexture(physicalResource);
		ASSERT_PTR(textureResource);

		VkImageLayout oldImageLayout = (prevUsage == nullptr) ? textureResource->GetLayout() : CalculateResourceImageLayout(*prevUsage, prevPassType);
		VkImageLayout srcImageLayout = CalculateResourceImageLayout(*currUsage, currPassType);
		
		// Special case for present resource. Also, finalLayout may not be UNDEFINED, so if nextUsage
		// is nullptr and it's not the present resource then we'll simply use the current layout as the final one
		VkImageLayout dstImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (nextUsage == nullptr)
		{
			if (physicalResource.resourceID == m_presentResID)
			{
				dstImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			else
			{
				dstImageLayout = srcImageLayout;
			}
		}
		else
		{
			dstImageLayout = CalculateResourceImageLayout(*nextUsage, nextPassType);
		}
		ASSERT_MSG(dstImageLayout != VK_IMAGE_LAYOUT_UNDEFINED, "Final layout may not be VK_IMAGE_LAYOUT_UNDEFINED");

		barrier.oldLayout = oldImageLayout;
		barrier.currentLayout = srcImageLayout;
		barrier.nextLayout = dstImageLayout;

		// If we perform a layout transition, we must set the appropriate dstAccessMask so the
		// layout write becomes visible
		barrier.dstAccessMask = CalculateResourceAccessFlags(*currUsage, physicalResource, currPassType);

		barrier.srcStageMask = (prevUsage == nullptr) ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : CalculateResourcePipelineStageFlags(prevPassType, VK_ACCESS_NONE, true);
		barrier.dstStageMask = CalculateResourcePipelineStageFlags(currPassType, barrier.dstAccessMask, false);

		return barrier;
	}

	STATUS_CODE RenderGraphVk::InsertResourceBarriers(const RenderPassVk& renderPass)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;
		DeviceContextVk* pDeviceContext = static_cast<DeviceContextVk*>(GetCurrentDeviceContext());

		for (auto iter : renderPass.m_barriers)
		{
			const Barrier& barrier = iter.second;
			const RenderResource& physicalResource = barrier.physicalResource;

			// GRAPHICS passes must only insert barriers for non-attachments (aka read resources)
			if (renderPass.m_passType == PASS_TYPE::GRAPHICS)
			{
				const ResourceUsage* currUsage = GetResourceUsageFromPass(physicalResource.resourceID, renderPass.m_index, 0);
				ASSERT_PTR(currUsage);

				if (IsWriteAccess(currUsage->access))
				{
					continue;
				}
			}

			switch (physicalResource.type)
			{
			case RESOURCE_TYPE::BUFFER:
			{
				BufferVk* pBuffer = ResolveBuffer(physicalResource);
				res = pDeviceContext->InsertBufferMemoryBarrier(
					pBuffer,
					ConvertPassTypeToQueueType(renderPass.m_passType),
					barrier.srcStageMask,
					barrier.dstStageMask,
					barrier.srcAccessMask,
					barrier.dstAccessMask
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
				AccelerationStructureVk* pAccelerationStructure = ResolveAccelerationStructure(physicalResource);
				res = pDeviceContext->InsertAccelerationStructureMemoryBarrier(
					pAccelerationStructure,
					ConvertPassTypeToQueueType(renderPass.m_passType),
					barrier.srcStageMask,
					barrier.dstStageMask,
					barrier.srcAccessMask,
					barrier.dstAccessMask
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
				TextureVk* pTexture = ResolveTexture(physicalResource);
				res = pDeviceContext->InsertImageMemoryBarrier(
					pTexture,
					ConvertPassTypeToQueueType(renderPass.m_passType),
					barrier.srcStageMask,
					barrier.dstStageMask,
					barrier.srcAccessMask,
					barrier.dstAccessMask,
					barrier.oldLayout,
					barrier.currentLayout
				);

				if (res != STATUS_CODE::SUCCESS)
				{
					LogError("Failed to insert dependency barriers. Could not insert image memory barrier!");
					return res;
				}

				// Update the texture's internal layout variable so it matches it's actual layout
				pTexture->SetLayout(barrier.currentLayout);
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

	void RenderGraphVk::TraverseDependencyTree(u32 renderPassIndex, TraversalDirection dir, TraverseDependenciesCallbackFn callback)
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

		auto& dependencyList = (dir == TraversalDirection::FORWARD) ? pCurrRenderPass->m_forwardDependencies : pCurrRenderPass->m_backwardDependencies;
		for (u32 i = 0; i < static_cast<u32>(dependencyList.size()); i++)
		{
			const DependencyInfo& dependeeInfo = dependencyList[i];
			ASSERT_PTR(dependeeInfo.renderPass);
			TraverseDependencyTree(dependeeInfo.renderPass->m_index, dir, callback);
		}
	}

	void RenderGraphVk::TraverseResources(const ResourceIndexBitset& resourceBitset, TraverseResourceCallbackFn callback) const
	{
		PROFILE_SCOPE("RenderGraphVk_TraverseResources");

		for (u32 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			if (resourceBitset.test(i))
			{
				const RenderResource& resource = m_physicalResources[i];
				callback(resource);
			}
		}
	}

	const ResourceUsage* RenderGraphVk::GetResourceUsageFromPass(ResourceID resourceId, u32 passIndex, i32 offset)
	{
		auto iter = m_resourcesUsageChain.find(resourceId);
		if (iter == m_resourcesUsageChain.end())
		{
			return false;
		}

		const ResourceUsage* requestedUsage = nullptr;
		auto& resourceUsageChain = iter->second;
		i32 chainSize = static_cast<i32>(resourceUsageChain.size());
		for (i32 i = 0; i < chainSize; i++)
		{
			const ResourceUsage& currUsage = resourceUsageChain[i];
			if (currUsage.passIndex == passIndex)
			{
				i32 adjustedIndex = i + offset;
				if (adjustedIndex >= 0 && adjustedIndex < chainSize)
				{
					// Valid usage found at offset
					requestedUsage = &resourceUsageChain[adjustedIndex];
					break;
				}
			}
		}

		return requestedUsage;
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
		PROFILE_SCOPE("RenderGraphVk_HashState");

		// If this changes it's likely that this function must be updated!
		STATIC_ASSERT(sizeof(RenderPassVk) == 896);

		size_t seed = 0;

		// Render passes
		HashCombine(seed, m_registeredRenderPasses.Size());
		for (u32 i = 0; i < static_cast<u32>(m_registeredRenderPasses.Size()); i++)
		{
			const RenderPassVk* pCurrRenderPass = m_registeredRenderPasses.Get(i);
			HashCombine(seed, pCurrRenderPass->m_readResources);
			HashCombine(seed, pCurrRenderPass->m_writeResources);
			// Ignore callbacks
			HashCombine(seed, pCurrRenderPass->m_index);

			HashCombine(seed, pCurrRenderPass->m_passType);
			switch (pCurrRenderPass->m_passType)
			{
			case PASS_TYPE::GRAPHICS:
			{
				const GraphicsPipelineDesc& pipelineDesc = std::get<GraphicsPipelineDesc>(pCurrRenderPass->m_pipelineDesc);
				HashCombine(seed, HashPipelineDesc(pipelineDesc, VK_NULL_HANDLE));
				break;
			}
			case PASS_TYPE::COMPUTE:
			{
				const ComputePipelineDesc& pipelineDesc = std::get<ComputePipelineDesc>(pCurrRenderPass->m_pipelineDesc);
				HashCombine(seed, HashPipelineDesc(pipelineDesc));
				break;
			}
			case PASS_TYPE::RAY_TRACING:
			{
				const RayTracingPipelineDesc& pipelineDesc = std::get<RayTracingPipelineDesc>(pCurrRenderPass->m_pipelineDesc);
				HashCombine(seed, HashPipelineDesc(pipelineDesc));
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

			// Barriers
			auto& barriers = pCurrRenderPass->m_barriers;
			HashCombine(seed, barriers.size());
			for (const auto& iter : barriers)
			{
				// Ignore barrierIter.first (resourceID) - may change per frame
				size_t currBarrierSeed = 0;
				const Barrier& barrier = iter.second;
				HashCombine(currBarrierSeed, barrier.dstAccessMask);
				HashCombine(currBarrierSeed, barrier.dstStageMask);
				HashCombine(currBarrierSeed, barrier.srcAccessMask);
				HashCombine(currBarrierSeed, barrier.srcStageMask);
				HashCombine(currBarrierSeed, barrier.oldLayout);
				HashCombine(currBarrierSeed, barrier.currentLayout);
				HashCombine(currBarrierSeed, barrier.nextLayout);

				// Uses a separate seed to keep the hash order-independent since we iterate over an unordered_map
				seed += currBarrierSeed;
			}

			HashCombine(seed, pCurrRenderPass->m_isVirtual);
		}

		// Resource usages
		HashCombine(seed, m_resourcesUsageChain.size());
		for (auto& it : m_resourcesUsageChain)
		{
			// Ignore resourceID - derived from handle, may change per frame
			size_t usageSeed = 0;
			auto& resourceUsageChain = it.second;
			for (const ResourceUsage& resourceUsage : resourceUsageChain)
			{
				// Ignore clearValue
				HashCombine(usageSeed, resourceUsage.access);
				HashCombine(usageSeed, resourceUsage.attachmentType);
				HashCombine(usageSeed, resourceUsage.storeOp);
				HashCombine(usageSeed, resourceUsage.loadOp);
				HashCombine(usageSeed, resourceUsage.bufferUsage);
				HashCombine(usageSeed, resourceUsage.passIndex);

				// Uses a separate seed to keep the hash order-independent since we iterate over an unordered_map
				seed += usageSeed;
			}
		}

		// Physical resources
		HashCombine(seed, m_physicalResources.size());
		for (u32 i = 0; i < static_cast<u32>(m_physicalResources.size()); i++)
		{
			const RenderResource& currResource = m_physicalResources[i];

			// Ignore resourceID - derived from handle, may change per frame
			HashCombine(seed, currResource.type);
		}

		return seed;
	}

	void RenderGraphVk::CallExecutionCallback(const RenderPassVk& renderPass, const DeviceContextHandle& deviceContext)
	{
		PROFILE_SCOPE("RenderGraphVk_CallExecutionCallback");

		if (renderPass.m_execCallback)
		{
			renderPass.m_execCallback(deviceContext);
		}
	}

	void RenderGraphVk::WriteBeginTimestamp(DeviceContextVk* pDeviceContext, const RenderPassVk& renderPass)
	{
		PROFILE_SCOPE("RenderGraphVk_WriteBeginTimestamp");

		// Begin GPU zone
		pDeviceContext->BeginZoneScope(renderPass.m_name);

		// Begin CPU zone
		if (GetSettings().gatherMetrics)
		{
			pDeviceContext->WriteBeginTimestamp(renderPass.m_name);
		}
	}

	void RenderGraphVk::WriteEndTimestamp(DeviceContextVk* pDeviceContext)
	{
		PROFILE_SCOPE("RenderGraphVk_WriteEndTimestamp");

		// End GPU zone
		pDeviceContext->EndZoneScope();

		// End CPU zone
		if (GetSettings().gatherMetrics)
		{
			pDeviceContext->WriteEndTimestamp();
		}
	}

}