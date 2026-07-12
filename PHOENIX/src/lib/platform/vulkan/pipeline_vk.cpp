
#include "pipeline_vk.h"

#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "framebuffer_vk.h"
#include "render_device_vk.h"
#include "shader_vk.h"
#include "uniform_vk.h"
#include "utils/buffer_utils.h"
#include "utils/logger.h"
#include "utils/pipeline_type_converter.h"
#include "utils/pipeline_utils.h"
#include "utils/render_pass_cache.h"
#include "utils/sanity.h"
#include "utils/texture_type_converter.h"
#include "utils/debug_utils.h"

namespace PHX
{
	static constexpr u32 SBT_REGION_COUNT = 4; // raygen, miss, hit, callable

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo) : 
		m_pRenderDevice(nullptr), m_pipeline(), m_layout(), m_bindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM), m_sbt(nullptr), 
		m_rayGenSBTRegion(), m_missSBTRegion(), m_hitSBTRegion(), m_callableSBTRegion()
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}
		m_pRenderDevice = pRenderDevice;

		CreateGraphicsPipeline(pRenderDevice, cache, renderPass, createInfo);
	}

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo) : 
		m_pRenderDevice(nullptr), m_pipeline(), m_layout(), m_bindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM), m_sbt(nullptr), 
		m_rayGenSBTRegion(), m_missSBTRegion(), m_hitSBTRegion(), m_callableSBTRegion()
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}
		m_pRenderDevice = pRenderDevice;

		CreateComputePipeline(pRenderDevice, cache, createInfo);
	}

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const RayTracingPipelineDesc& createInfo) :
		m_pRenderDevice(nullptr), m_pipeline(), m_layout(), m_bindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM), m_sbt(nullptr), 
		m_rayGenSBTRegion(), m_missSBTRegion(), m_hitSBTRegion(), m_callableSBTRegion()
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}
		m_pRenderDevice = pRenderDevice;

		CreateRayTracingPipeline(pRenderDevice, cache, createInfo);
	}

	PipelineVk::~PipelineVk()
	{
		if (m_pRenderDevice == nullptr)
		{
			return;
		}

		vkDestroyPipeline(m_pRenderDevice->GetLogicalDevice(), m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_pRenderDevice->GetLogicalDevice(), m_layout, nullptr);

		if (m_sbt != nullptr)
		{
			DestroyBuffer(m_pRenderDevice, *m_sbt);
			SAFE_DEL(m_sbt);
		}
	}

	const VkStridedDeviceAddressRegionKHR* PipelineVk::GetRayGenSBTRegion() const
	{
		return &m_rayGenSBTRegion;
	}

	const VkStridedDeviceAddressRegionKHR* PipelineVk::GetMissSBTRegion() const
	{
		return &m_missSBTRegion;
	}

	const VkStridedDeviceAddressRegionKHR* PipelineVk::GetHitSBTRegion() const
	{
		return &m_hitSBTRegion;
	}

	const VkStridedDeviceAddressRegionKHR* PipelineVk::GetCallableSBTRegion() const
	{
		return &m_callableSBTRegion;
	}

	VkPipeline PipelineVk::GetPipeline() const
	{
		return m_pipeline;
	}

	VkPipelineLayout PipelineVk::GetLayout() const
	{
		return m_layout;
	}

	VkPipelineBindPoint PipelineVk::GetBindPoint() const
	{
		return m_bindPoint;
	}

	STATUS_CODE PipelineVk::CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo)
	{
		STATUS_CODE createInfoRes = VerifyCreateInfo(createInfo);
		if (createInfoRes != STATUS_CODE::SUCCESS)
		{
			return createInfoRes;
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		m_layout = CreatePipelineLayout(logicalDevice, createInfo.uniformCollection);
		if (m_layout == VK_NULL_HANDLE)
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.reserve(createInfo.shaderCount);
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			ShaderVk* pShader = static_cast<ShaderVk*>(m_pRenderDevice->ResolveHandle(createInfo.pShaders[i]));
			shaderStages.emplace_back(PopulateShaderCreateInfo(pShader));
		}

		// Vertex input layout (optional)
		std::vector<VkVertexInputAttributeDescription> inputAttributeDescs;
		std::vector<VkVertexInputBindingDescription> inputBindingDescs;
		bool usesInputAttributes = (createInfo.pInputAttributes != nullptr) && (createInfo.attributeCount != 0);
		if (usesInputAttributes)
		{
			PopulateInputAttributeDescription(createInfo.pInputAttributes, createInfo.attributeCount, inputAttributeDescs);
			PopulateInputBindingDescription(createInfo.pInputAttributes, createInfo.attributeCount, createInfo.inputBinding, createInfo.inputRate, inputBindingDescs);
		}

		// TODO - Should we always have the viewport and scissor as dynamic states? Should it be adjustable?
		const u32 NUM_DYNAMIC_STATES = 2;
		const VkDynamicState dynamicStates[NUM_DYNAMIC_STATES] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		// Core pipeline descriptions
		VkPipelineVertexInputStateCreateInfo		vertexInputInfo      = PopulateVertexInputCreateInfo(inputBindingDescs, inputAttributeDescs);
		VkPipelineInputAssemblyStateCreateInfo		inputAssembly        = PopulateInputAssemblyCreateInfo(PIPELINE_UTILS::ConvertPrimitiveTopology(createInfo.topology), createInfo.enableRestartPrimitives);
		VkViewport									viewport             = PopulateViewportInfo(createInfo.viewportSize, createInfo.viewportDepthRange);
		VkRect2D									scissor              = PopulateScissorInfo(createInfo.scissorOffset, createInfo.scissorExtent);
		VkPipelineDynamicStateCreateInfo			dynamicState         = PopulateDynamicStateCreateInfo(&dynamicStates[0], NUM_DYNAMIC_STATES);
		VkPipelineViewportStateCreateInfo			viewportState        = PopulateViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineMultisampleStateCreateInfo		multisampling        = PopulateMultisamplingStateCreateInfo(TEX_UTILS::ConvertSampleCount(createInfo.rasterizationSamples), createInfo.enableAlphaToCoverage, createInfo.enableAlphaToOne);
		VkPipelineColorBlendAttachmentState			colorBlendAttachment = PopulateColorBlendAttachment(
			createInfo.blendState.enableBlend ? VK_TRUE : VK_FALSE,
			PIPELINE_UTILS::ConvertBlendFactor(createInfo.blendState.srcColorFactor),
			PIPELINE_UTILS::ConvertBlendFactor(createInfo.blendState.dstColorFactor),
			PIPELINE_UTILS::ConvertBlendOp(createInfo.blendState.colorBlendOp),
			PIPELINE_UTILS::ConvertBlendFactor(createInfo.blendState.srcAlphaFactor),
			PIPELINE_UTILS::ConvertBlendFactor(createInfo.blendState.dstAlphaFactor),
			PIPELINE_UTILS::ConvertBlendOp(createInfo.blendState.alphaBlendOp),
			PIPELINE_UTILS::ConvertColorComponentFlags(createInfo.blendState.colorWriteMask));
		VkPipelineColorBlendStateCreateInfo			colorBlending        = PopulateColorBlendStateCreateInfo(&colorBlendAttachment, 1);
		VkPipelineDepthStencilStateCreateInfo		depthStencil         = PopulateDepthStencilStateCreateInfo(createInfo.enableDepthTest, 
			createInfo.enableDepthWrite, 
			PIPELINE_UTILS::ConvertCompareOp(createInfo.compareOp), 
			createInfo.enableDepthBoundsTest, 
			createInfo.depthBoundsRange, 
			createInfo.enableStencilTest, 
			createInfo.stencilFront, 
			createInfo.stencilBack);
		VkPipelineRasterizationStateCreateInfo		rasterizer           = PopulateRasterizerStateCreateInfo(PIPELINE_UTILS::ConvertCullMode(createInfo.cullMode), 
			PIPELINE_UTILS::ConvertFrontFaceWinding(createInfo.frontFaceWinding), 
			PIPELINE_UTILS::ConvertPolygonMode(createInfo.polygonMode), 
			createInfo.lineWidth, 
			createInfo.enableDepthClamp, 
			createInfo.enableRasterizerDiscard, 
			createInfo.enableDepthBias, 
			createInfo.depthBiasConstantFactor, 
			createInfo.depthBiasClamp, 
			createInfo.depthBiasSlopeFactor);

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<u32>(shaderStages.size());
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_layout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		VkResult res = vkCreateGraphicsPipelines(logicalDevice, cache, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create pipeline! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(m_pipeline), "GraphicsPipeline");
		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(m_layout), "GraphicsPipelineLayout");

		m_bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		LogDebug("GRAPHICS PIPELINE CREATED: %u stages", pipelineInfo.stageCount);
		for (u32 i = 0; i < pipelineInfo.stageCount; i++)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = shaderStages[i];
			LogDebug("\tShader stage %u (%s)", i, string_VkShaderStageFlagBits(shaderStage.stage));
			LogDebug("\t- Name: %s", shaderStage.pName);
		}
		LogDebug("\tPrimitive topology:    %s", string_VkPrimitiveTopology(inputAssembly.topology));
		LogDebug("\tViewport position:     (%2.3f, %2.3f)", viewport.x, viewport.y);
		LogDebug("\tViewport size:         (%2.3f, %2.3f)", viewport.width, viewport.height);
		LogDebug("\tScissor position:      (%i, %i)", scissor.offset.x, scissor.offset.y);
		LogDebug("\tScissor size:          (%u, %u)", scissor.extent.width, scissor.extent.height);
		LogDebug("\tMultisampling samples: %u", multisampling.rasterizationSamples);
		LogDebug("\tFront face:            %s", string_VkFrontFace(rasterizer.frontFace));
		LogDebug("\tPolygon mode:          %s", string_VkPolygonMode(rasterizer.polygonMode));
		LogDebug("\tLine width:            %u", rasterizer.lineWidth);
		LogDebug("\tRasterizer discard:    %s", rasterizer.rasterizerDiscardEnable ? "true" : "false");
		LogDebug("\tLayout ptr:            %p", pipelineInfo.layout);
		LogDebug("\tRender pass:           %p", pipelineInfo.renderPass);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::CreateComputePipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo)
	{
		STATUS_CODE createInfoRes = VerifyCreateInfo(createInfo);
		if (createInfoRes != STATUS_CODE::SUCCESS)
		{
			return createInfoRes;
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		m_layout = CreatePipelineLayout(logicalDevice, createInfo.uniformCollection);
		if (m_layout == VK_NULL_HANDLE)
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		ShaderVk* pShader = static_cast<ShaderVk*>(m_pRenderDevice->ResolveHandle(createInfo.shader));

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = m_layout;
		pipelineInfo.stage = PopulateShaderCreateInfo(pShader);

		VkResult res = vkCreateComputePipelines(logicalDevice, cache, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create compute pipeline! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(m_pipeline), "ComputePipeline");
		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(m_layout), "ComputePipelineLayout");

		m_bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		LogDebug("COMPUTE PIPELINE CREATED");
		LogDebug("\tShader name: %s", pipelineInfo.stage.pName);
		LogDebug("\tLayout ptr:  %p", pipelineInfo.layout);

		return STATUS_CODE::SUCCESS;
	}

	bool PipelineVk::IsRayTracingShaderStage(SHADER_STAGE stage) const
	{
		return (stage >= SHADER_STAGE::RAYGEN) && (stage < SHADER_STAGE::MAX);
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const RayTracingPipelineDesc& createInfo)
	{
		ASSERT_MSG(createInfo.pShaders != nullptr, "Failed to create ray tracing pipeline! Shaders array is null");
		ASSERT_MSG(createInfo.shaderCount != 0, "Failed to create ray tracing pipeline! Shader count is 0");

		bool hasRaygen = false;
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			ShaderHandle currShader = createInfo.pShaders[i];
			if (currShader == INVALID_HANDLE)
			{
				LogError("Failed to create ray tracing pipeline. Shader handle at index %u is invalid!", i);
				return STATUS_CODE::ERR_API;
			}

			ShaderVk* pShader = static_cast<ShaderVk*>(m_pRenderDevice->ResolveHandle(currShader));
			if (pShader == nullptr)
			{
				LogError("Failed to create ray tracing pipeline. Could not resolve shader at index %u!", i);
				return STATUS_CODE::ERR_API;
			}

			if (!IsRayTracingShaderStage(pShader->GetStage()))
			{
				LogError("Failed to create ray tracing pipeline. Shader at index %u is not a ray tracing shader!", i);
				return STATUS_CODE::ERR_API;
			}

			if (pShader->GetStage() == SHADER_STAGE::RAYGEN)
			{
				hasRaygen = true;
			}
		}

		if (!hasRaygen)
		{
			LogError("Failed to create ray tracing pipeline. A raygen shader is required!");
			return STATUS_CODE::ERR_API;
		}

		// Check set and binding numbers against device limits, if applicable
		if (createInfo.uniformCollection.IsValid())
		{
			const u32 maxBoundDescriptors = m_pRenderDevice->GetDeviceProperties().limits.maxBoundDescriptorSets;
			const u32 groupCount = createInfo.uniformCollection.GetGroupCount();

			for (u32 i = 0; i < groupCount; i++)
			{
				const UniformDataGroup* const dataGroup = createInfo.uniformCollection.GetGroup(i);
				const u32 dataGroupSetNumber = dataGroup->set;
				if (dataGroupSetNumber > maxBoundDescriptors)
				{
					LogWarning("Attempting to create ray tracing pipeline, but the set of a uniform data group (%u) exceeds the maximum allowed by the device (%u)", dataGroupSetNumber, maxBoundDescriptors);
				}

				const u32 uniformArrayCount = dataGroup->uniformArrayCount;
				for (u32 j = 0; j < uniformArrayCount; j++)
				{
					const UniformData& uniformData = dataGroup->uniformArray[j];

					const u32 shaderStage = static_cast<u32>(uniformData.shaderStage);
					if (shaderStage >= static_cast<u32>(SHADER_STAGE::MAX))
					{
						LogWarning("Attempting to create a ray tracing pipeline, but the shader stage from data group %u at index %u is invalid (%i)", i, j, shaderStage);
					}

					const u32 uniformType = static_cast<u32>(uniformData.type);
					if (uniformType >= static_cast<u32>(UNIFORM_TYPE::MAX))
					{
						LogWarning("Attempting to create a ray tracing pipeline, but the type from data group %u at index %u is invalid (%u)", i, j, uniformType);
					}
				}
			}
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::CreateRayTracingPipeline(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const RayTracingPipelineDesc& createInfo)
	{
		STATUS_CODE res = STATUS_CODE::SUCCESS;
		VkResult vkRes = VK_SUCCESS;

		if (!pRenderDevice->IsRayTracingSupported())
		{
			LogError("Cannot create a ray tracing pipeline because the device does not support ray tracing!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		res = VerifyCreateInfo(createInfo);
		if (res != STATUS_CODE::SUCCESS)
		{
			return res;
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		m_layout = CreatePipelineLayout(logicalDevice, createInfo.uniformCollection);
		if (m_layout == VK_NULL_HANDLE)
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Build shader stages in the order provided by the description
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		std::vector<SHADER_STAGE> shaderStageTypes;
		shaderStages.reserve(createInfo.shaderCount);
		shaderStageTypes.reserve(createInfo.shaderCount);

		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			ShaderVk* pShader = static_cast<ShaderVk*>(m_pRenderDevice->ResolveHandle(createInfo.pShaders[i]));
			if (pShader == nullptr)
			{
				LogError("Failed to create ray tracing pipeline! Could not resolve shader at index %u", i);
				return STATUS_CODE::ERR_INTERNAL;
			}

			shaderStages.emplace_back(PopulateShaderCreateInfo(pShader));
			shaderStageTypes.emplace_back(pShader->GetStage());
		}

		// Build shader groups ordered by type so the SBT layout is [raygen][miss][hit][callable]
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
		u32 raygenGroupCount = 0;
		u32 missGroupCount = 0;
		u32 hitGroupCount = 0;
		u32 callableGroupCount = 0;

		auto AddGroup = [&](VkRayTracingShaderGroupTypeKHR type, u32 generalShader, u32 closestHitShader, u32 anyHitShader, u32 intersectionShader)
		{
			VkRayTracingShaderGroupCreateInfoKHR group{};
			group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			group.type = type;
			group.generalShader = generalShader;
			group.closestHitShader = closestHitShader;
			group.anyHitShader = anyHitShader;
			group.intersectionShader = intersectionShader;
			group.pShaderGroupCaptureReplayHandle = nullptr;
			shaderGroups.push_back(group);
		};

		auto AddStageGroup = [&](u32 stageIndex, SHADER_STAGE stage)
		{
			switch (stage)
			{
			case SHADER_STAGE::RAYGEN:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, stageIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
				raygenGroupCount++;
				break;
			case SHADER_STAGE::MISS:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, stageIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
				missGroupCount++;
				break;
			case SHADER_STAGE::CALLABLE:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, stageIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
				callableGroupCount++;
				break;
			case SHADER_STAGE::CLOSEST_HIT:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, stageIndex, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR);
				hitGroupCount++;
				break;
			case SHADER_STAGE::ANY_HIT:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, stageIndex, VK_SHADER_UNUSED_KHR);
				hitGroupCount++;
				break;
			case SHADER_STAGE::INTERSECTION:
				AddGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, stageIndex);
				hitGroupCount++;
				break;
			default:
				LogError("Failed to create ray tracing pipeline. Shader stage %u is not a valid ray tracing stage!", static_cast<u32>(stage));
				return STATUS_CODE::ERR_API;
			}

			return STATUS_CODE::SUCCESS;
		};

		// Raygen groups first
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			if (shaderStageTypes[i] == SHADER_STAGE::RAYGEN)
			{
				res = AddStageGroup(i, shaderStageTypes[i]);
				if (res != STATUS_CODE::SUCCESS)
				{
					return res;
				}
			}
		}

		// Miss groups
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			if (shaderStageTypes[i] == SHADER_STAGE::MISS)
			{
				res = AddStageGroup(i, shaderStageTypes[i]);
				if (res != STATUS_CODE::SUCCESS)
				{
					return res;
				}
			}
		}

		// Hit groups
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			if (shaderStageTypes[i] == SHADER_STAGE::CLOSEST_HIT || shaderStageTypes[i] == SHADER_STAGE::ANY_HIT || shaderStageTypes[i] == SHADER_STAGE::INTERSECTION)
			{
				res = AddStageGroup(i, shaderStageTypes[i]);
				if (res != STATUS_CODE::SUCCESS)
				{
					return res;
				}
			}
		}

		// Callable groups
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			if (shaderStageTypes[i] == SHADER_STAGE::CALLABLE)
			{
				res = AddStageGroup(i, shaderStageTypes[i]);
				if (res != STATUS_CODE::SUCCESS)
				{
					return res;
				}
			}
		}

		const u32 groupCount = static_cast<u32>(shaderGroups.size());

		VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		pipelineInfo.stageCount = static_cast<u32>(shaderStages.size());
		pipelineInfo.pStages = shaderStages.data();
		pipelineInfo.groupCount = groupCount;
		pipelineInfo.pGroups = shaderGroups.data();
		pipelineInfo.maxPipelineRayRecursionDepth = 1;
		pipelineInfo.layout = m_layout;
		
		vkRes = pRenderDevice->CreateRayTracingPipelinesKHR(cache, pipelineInfo, &m_pipeline);
		if (vkRes != VK_SUCCESS)
		{
			LogError("Failed to create ray tracing pipeline! Got error: \"%s\"", string_VkResult(vkRes));
			return STATUS_CODE::ERR_INTERNAL;
		}

		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(m_pipeline), "RayTracingPipeline");
		DEBUG_UTILS::SetObjectName(logicalDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, reinterpret_cast<uint64_t>(m_layout), "RayTracingPipelineLayout");

		// Build the shader binding table
		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps = pRenderDevice->GetRayTracingPipelineProperties();
		const u32 handleSize = rtProps.shaderGroupHandleSize;
		const u32 handleAlignment = rtProps.shaderGroupHandleAlignment;
		const u32 handleStride = ((handleSize + handleAlignment - 1) / handleAlignment) * handleAlignment;
		const u32 baseAlignment = rtProps.shaderGroupBaseAlignment;

		const u64 handleDataSize = static_cast<u64>(handleSize) * groupCount;
		std::vector<u8> groupHandles(handleDataSize);

		vkRes = pRenderDevice->GetRayTracingShaderGroupHandlesKHR(m_pipeline, 0, groupCount, handleDataSize, groupHandles.data());
		if (vkRes != VK_SUCCESS)
		{
			LogError("Failed to get ray tracing shader group handles! Got error: \"%s\"", string_VkResult(vkRes));
			return STATUS_CODE::ERR_INTERNAL;
		}

		auto AlignUp = [](u64 value, u64 alignment) -> u64
		{
			return ((value + alignment - 1) / alignment) * alignment;
		};

		// Determine each region's starting offset (relative to the aligned base address).
		// Each region's deviceAddress must be a multiple of shaderGroupBaseAlignment.
		u64 regionOffsets[SBT_REGION_COUNT] = {};
		u32 regionGroupCounts[SBT_REGION_COUNT] = { raygenGroupCount, missGroupCount, hitGroupCount, callableGroupCount };
		u64 currentOffset = 0;
		for (u32 i = 0; i < SBT_REGION_COUNT; i++)
		{
			if (regionGroupCounts[i] > 0)
			{
				currentOffset = AlignUp(currentOffset, static_cast<u64>(baseAlignment));
				regionOffsets[i] = currentOffset;
				currentOffset += static_cast<u64>(regionGroupCounts[i]) * handleStride;
			}
		}

		const u64 sbtSize = baseAlignment + currentOffset;
		const VkBufferUsageFlags sbtBufferUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		const VmaAllocationCreateFlags sbtAllocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		BufferData sbtBuffer = CreateBuffer(pRenderDevice, "ShaderBindingTableBuffer", sbtSize, sbtBufferUsage, sbtAllocFlags, 0, 0);
		if (!sbtBuffer.isValid)
		{
			LogError("Failed to create ray tracing shader binding table buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = sbtBuffer.buffer;
		VkDeviceAddress baseAddress = pRenderDevice->GetBufferDeviceAddressKHR(&addressInfo);

		// Align the base address to shaderGroupBaseAlignment
		const u64 alignedBaseAddress = AlignUp(baseAddress, static_cast<u64>(baseAlignment));
		const u64 baseOffset = alignedBaseAddress - baseAddress;

		u8* mappedSBT = static_cast<u8*>(sbtBuffer.allocInfo.pMappedData);
		u32 groupHandleIndex = 0;
		for (u32 i = 0; i < SBT_REGION_COUNT; i++)
		{
			for (u32 j = 0; j < regionGroupCounts[i]; j++)
			{
				u8* dst = mappedSBT + baseOffset + regionOffsets[i] + (static_cast<u64>(j) * handleStride);
				const u8* src = groupHandles.data() + (static_cast<u64>(groupHandleIndex) * handleSize);
				memcpy(dst, src, handleSize);
				groupHandleIndex++;
			}
		}

		m_sbt = new BufferData(sbtBuffer);

		m_rayGenSBTRegion.deviceAddress = (raygenGroupCount > 0) ? (alignedBaseAddress + regionOffsets[0]) : 0;
		m_rayGenSBTRegion.stride = handleStride;
		m_rayGenSBTRegion.size = static_cast<VkDeviceSize>(raygenGroupCount) * handleSize;

		m_missSBTRegion.deviceAddress = (missGroupCount > 0) ? (alignedBaseAddress + regionOffsets[1]) : 0;
		m_missSBTRegion.stride = handleStride;
		m_missSBTRegion.size = static_cast<VkDeviceSize>(missGroupCount) * handleStride;

		m_hitSBTRegion.deviceAddress = (hitGroupCount > 0) ? (alignedBaseAddress + regionOffsets[2]) : 0;
		m_hitSBTRegion.stride = handleStride;
		m_hitSBTRegion.size = static_cast<VkDeviceSize>(hitGroupCount) * handleStride;

		m_callableSBTRegion.deviceAddress = (callableGroupCount > 0) ? (alignedBaseAddress + regionOffsets[3]) : 0;
		m_callableSBTRegion.stride = handleStride;
		m_callableSBTRegion.size = static_cast<VkDeviceSize>(callableGroupCount) * handleStride;

		m_bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

		LogDebug("RAY TRACING PIPELINE CREATED");
		LogDebug("\tShader stages: %u", pipelineInfo.stageCount);
		LogDebug("\tShader groups: %u", pipelineInfo.groupCount);
		LogDebug("\tRaygen groups: %u", raygenGroupCount);
		LogDebug("\tMiss groups: %u", missGroupCount);
		LogDebug("\tHit groups: %u", hitGroupCount);
		LogDebug("\tCallable groups: %u", callableGroupCount);
		LogDebug("\tHandle size: %u", handleSize);
		LogDebug("\tHandle stride: %u", handleStride);
		LogDebug("\tSBT size: %llu", sbtSize);
		LogDebug("\tLayout ptr: %p", pipelineInfo.layout);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const GraphicsPipelineDesc& createInfo)
	{
		// Render graph prevents pipelines without shaders from being created!
		ASSERT_MSG(createInfo.pShaders != nullptr, "Failed to create graphics pipeline! Shaders array is null");
		ASSERT_MSG(createInfo.shaderCount != 0, "Failed to create graphics pipeline! Shader count is 0");

		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			ShaderHandle currShader = createInfo.pShaders[i];
			if (currShader == INVALID_HANDLE)
			{
				LogError("Failed to create graphics pipeline. Shader handle at index %u is invalid!", i);
				return STATUS_CODE::ERR_API;
			}
		}

		// Check set and binding numbers against device limits, if applicable
		if (createInfo.uniformCollection.IsValid())
		{
			const u32 maxBoundDescriptors = m_pRenderDevice->GetDeviceProperties().limits.maxBoundDescriptorSets;
			const u32 groupCount = createInfo.uniformCollection.GetGroupCount();

			for (u32 i = 0; i < groupCount; i++)
			{
				const UniformDataGroup* const dataGroup = createInfo.uniformCollection.GetGroup(i);
				const u32 dataGroupSetNumber = dataGroup->set;
				if (dataGroupSetNumber > maxBoundDescriptors)
				{
					LogWarning("Attempting to create graphics pipeline, but the set of a uniform data group (%u) exceeds the maximum allowed by the device (%u)", dataGroupSetNumber, maxBoundDescriptors);
				}

				const u32 uniformArrayCount = dataGroup->uniformArrayCount;
				for (u32 j = 0; j < uniformArrayCount; j++)
				{
					const UniformData& uniformData = dataGroup->uniformArray[j];
					
					const u32 shaderStage = static_cast<u32>(uniformData.shaderStage);
					if (shaderStage >= static_cast<u32>(SHADER_STAGE::MAX))
					{
						LogWarning("Attempting to create a graphics pipeline, but the shader stage from data group %u at index %u is invalid (%i)", i, j, shaderStage);
					}

					const u32 uniformType = static_cast<u32>(uniformData.type);
					if (uniformType >= static_cast<u32>(UNIFORM_TYPE::MAX))
					{
						LogWarning("Attempting to create a graphics pipeline, but the type from data group %u at index %u is invalid (%u)", i, j, uniformType);
					}
				}
			}
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const ComputePipelineDesc& createInfo)
	{
		if (createInfo.shader == INVALID_HANDLE)
		{
			LogError("Failed to create compute pipeline! Shader handle is invalid");
			return STATUS_CODE::ERR_API;
		}

		// Check set and binding numbers against device limits, if applicable
		if (createInfo.uniformCollection.IsValid())
		{
			const u32 maxBoundDescriptors = m_pRenderDevice->GetDeviceProperties().limits.maxBoundDescriptorSets;
			const u32 groupCount = createInfo.uniformCollection.GetGroupCount();

			for (u32 i = 0; i < groupCount; i++)
			{
				const UniformDataGroup* const dataGroup = createInfo.uniformCollection.GetGroup(i);
				const u32 dataGroupSetNumber = dataGroup->set;
				if (dataGroupSetNumber > maxBoundDescriptors)
				{
					LogWarning("Attempting to create compute pipeline, but the set of a uniform data group (%u) exceeds the maximum allowed by the device (%u)", dataGroupSetNumber, maxBoundDescriptors);
				}

				const u32 uniformArrayCount = dataGroup->uniformArrayCount;
				for (u32 j = 0; j < uniformArrayCount; j++)
				{
					const UniformData& uniformData = dataGroup->uniformArray[j];

					const u32 shaderStage = static_cast<u32>(uniformData.shaderStage);
					if (shaderStage >= static_cast<u32>(SHADER_STAGE::MAX))
					{
						LogWarning("Attempting to create a compute pipeline, but the shader stage from data group %u at index %u is invalid (%i)", i, j, shaderStage);
					}

					const u32 uniformType = static_cast<u32>(uniformData.type);
					if (uniformType >= static_cast<u32>(UNIFORM_TYPE::MAX))
					{
						LogWarning("Attempting to create a compute pipeline, but the type from data group %u at index %u is invalid (%u)", i, j, uniformType);
					}
				}
			}
		}

		return STATUS_CODE::SUCCESS;
	}

	VkPipelineLayout PipelineVk::CreatePipelineLayout(VkDevice logicalDevice, UniformCollectionHandle uniformCollection)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		if (uniformCollection.IsValid())
		{
			// TODO - Add support for push constants
			UniformCollectionVk* pUniformCollectionVk = static_cast<UniformCollectionVk*>(m_pRenderDevice->ResolveHandle(uniformCollection));
			pipelineLayoutInfo = PopulatePipelineLayoutCreateInfo(pUniformCollectionVk->GetDescriptorSetLayouts(), pUniformCollectionVk->GetDescriptorSetLayoutCount(), nullptr, 0);
		}
		else
		{
			pipelineLayoutInfo = PopulatePipelineLayoutCreateInfo(nullptr, 0, nullptr, 0);
		}

		VkPipelineLayout layout;
		VkResult res = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &layout);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create pipeline layout! Got error: \"%s\"", string_VkResult(res));
			return VK_NULL_HANDLE;
		}

		return layout;
	}
}