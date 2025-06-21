
#include "pipeline_vk.h"

#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "framebuffer_vk.h"
#include "render_device_vk.h"
#include "uniform_vk.h"
#include "utils/logger.h"
#include "utils/pipeline_type_converter.h"
#include "utils/pipeline_utils.h"
#include "utils/render_pass_cache.h"
#include "utils/sanity.h"
#include "utils/texture_type_converter.h"

namespace PHX
{

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, VkRenderPass renderPass, const GraphicsPipelineDesc& createInfo) : 
		m_pRenderDevice(nullptr), m_pipeline(), m_layout(), m_bindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}
		m_pRenderDevice = pRenderDevice;

		CreateGraphicsPipeline(pRenderDevice, cache, renderPass, createInfo);
	}

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, VkPipelineCache cache, const ComputePipelineDesc& createInfo) : 
		m_pRenderDevice(nullptr), m_pipeline(), m_layout(), m_bindPoint(VK_PIPELINE_BIND_POINT_MAX_ENUM)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}
		m_pRenderDevice = pRenderDevice;

		CreateComputePipeline(pRenderDevice, cache, createInfo);
	}

	PipelineVk::~PipelineVk()
	{
		if (m_pRenderDevice == nullptr)
		{
			return;
		}

		vkDestroyPipeline(m_pRenderDevice->GetLogicalDevice(), m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_pRenderDevice->GetLogicalDevice(), m_layout, nullptr);
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

		m_layout = CreatePipelineLayout(logicalDevice, createInfo.pUniformCollection);
		if (m_layout == VK_NULL_HANDLE)
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		// Shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.reserve(createInfo.shaderCount);
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			shaderStages.emplace_back(PopulateShaderCreateInfo(createInfo.ppShaders[i]));
		}

		// Vertex input layout
		std::vector<VkVertexInputAttributeDescription> inputAttributeDescs;
		PopulateInputAttributeDescription(createInfo.pInputAttributes, createInfo.attributeCount, inputAttributeDescs);
		VkVertexInputBindingDescription inputBindingDesc = PopulateInputBindingDescription(createInfo.pInputAttributes, createInfo.attributeCount, createInfo.inputBinding, createInfo.inputRate);

		// TODO - Should we always have the viewport and scissor as dynamic states? Should it be adjustable?
		const u32 NUM_DYNAMIC_STATES = 2;
		const VkDynamicState dynamicStates[NUM_DYNAMIC_STATES] =
		{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
		};

		// Core pipeline descriptions
		VkPipelineVertexInputStateCreateInfo		vertexInputInfo      = PopulateVertexInputCreateInfo(inputBindingDesc, inputAttributeDescs);
		VkPipelineInputAssemblyStateCreateInfo		inputAssembly        = PopulateInputAssemblyCreateInfo(PIPELINE_UTILS::ConvertPrimitiveTopology(createInfo.topology), createInfo.enableRestartPrimitives);
		VkViewport									viewport             = PopulateViewportInfo(createInfo.viewportSize, createInfo.viewportDepthRange);
		VkRect2D									scissor              = PopulateScissorInfo(createInfo.scissorOffset, createInfo.scissorExtent);
		VkPipelineDynamicStateCreateInfo			dynamicState         = PopulateDynamicStateCreateInfo(&dynamicStates[0], NUM_DYNAMIC_STATES);
		VkPipelineViewportStateCreateInfo			viewportState        = PopulateViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineMultisampleStateCreateInfo		multisampling        = PopulateMultisamplingStateCreateInfo(TEX_UTILS::ConvertSampleCount(createInfo.rasterizationSamples), createInfo.enableAlphaToCoverage, createInfo.enableAlphaToOne);
		VkPipelineColorBlendAttachmentState			colorBlendAttachment = PopulateColorBlendAttachment();
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

		m_bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		LogDebug("GRAPHICS PIPELINE CREATED: %u stages", pipelineInfo.stageCount);
		for (u32 i = 0; i < pipelineInfo.stageCount; i++)
		{
			const VkPipelineShaderStageCreateInfo& shaderStage = shaderStages[i];
			LogDebug("\tShader stage %u (%s)", i, string_VkShaderStageFlagBits(shaderStage.stage));
			LogDebug("\t- Name: %s", shaderStage.pName);
		}
		LogDebug("\tPrimitive topology: %s", string_VkPrimitiveTopology(inputAssembly.topology));
		LogDebug("\tViewport position: (%2.3f, %2.3f)", viewport.x, viewport.y);
		LogDebug("\tViewport size: (%2.3f, %2.3f)", viewport.width, viewport.height);
		LogDebug("\tScissor position: (%i, %i)", scissor.offset.x, scissor.offset.y);
		LogDebug("\tScissor size: (%u, %u)", scissor.extent.width, scissor.extent.height);
		LogDebug("\tMultisampling samples: %u", multisampling.rasterizationSamples);
		LogDebug("\tFront face: %s", string_VkFrontFace(rasterizer.frontFace));
		LogDebug("\tPolygon mode: %s", string_VkPolygonMode(rasterizer.polygonMode));
		LogDebug("\tLine width: %u", rasterizer.lineWidth);
		LogDebug("\tRasterizer discard: %s", rasterizer.rasterizerDiscardEnable ? "true" : "false");
		LogDebug("\tLayout ptr: %p", pipelineInfo.layout);
		LogDebug("\tRender pass: %p", pipelineInfo.renderPass);

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

		m_layout = CreatePipelineLayout(logicalDevice, createInfo.pUniformCollection);
		if (m_layout == VK_NULL_HANDLE)
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.layout = m_layout;
		pipelineInfo.stage = PopulateShaderCreateInfo(createInfo.pShader);

		VkResult res = vkCreateComputePipelines(logicalDevice, cache, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create compute pipeline! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		LogDebug("COMPUTE PIPELINE CREATED");
		LogDebug("\tShader name: %s", pipelineInfo.stage.pName);
		LogDebug("\tLayout ptr: %p", pipelineInfo.layout);

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const GraphicsPipelineDesc& createInfo)
	{
		if (createInfo.pInputAttributes == nullptr)
		{
			LogError("Failed to create graphics pipeline! Input attributes are null");
			return STATUS_CODE::ERR_API;
		}
		if (createInfo.ppShaders == nullptr)
		{
			LogError("Failed to create graphics pipeline! Shaders array is null");
			return STATUS_CODE::ERR_API;
		}
		if (createInfo.shaderCount == 0)
		{
			LogError("Failed to create graphics pipeline! Shader count is 0");
			return STATUS_CODE::ERR_API;
		}

		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			const IShader* pShader = createInfo.ppShaders[i];
			if (pShader == nullptr)
			{
				LogError("Failed to create graphics pipeline! Shader at index %u is null", i);
				return STATUS_CODE::ERR_API;
			}
		}

		// Check set and binding numbers against device limits, if applicable
		if (createInfo.pUniformCollection != nullptr)
		{
			const u32 maxBoundDescriptors = m_pRenderDevice->GetDeviceProperties().limits.maxBoundDescriptorSets;
			const u32 groupCount = createInfo.pUniformCollection->GetGroupCount();

			for (u32 i = 0; i < groupCount; i++)
			{
				const UniformDataGroup* const dataGroup = createInfo.pUniformCollection->GetGroup(i);
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
		if (createInfo.pShader == nullptr)
		{
			LogError("Failed to create compute pipeline! Shader is null");
			return STATUS_CODE::ERR_API;
		}

		// Check set and binding numbers against device limits, if applicable
		if (createInfo.pUniformCollection != nullptr)
		{
			const u32 maxBoundDescriptors = m_pRenderDevice->GetDeviceProperties().limits.maxBoundDescriptorSets;
			const u32 groupCount = createInfo.pUniformCollection->GetGroupCount();

			for (u32 i = 0; i < groupCount; i++)
			{
				const UniformDataGroup* const dataGroup = createInfo.pUniformCollection->GetGroup(i);
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

	VkPipelineLayout PipelineVk::CreatePipelineLayout(VkDevice logicalDevice, IUniformCollection* pUniformCollection)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		if (pUniformCollection != nullptr)
		{
			// TODO - Add support for push constants
			UniformCollectionVk* pUniformCollectionVk = static_cast<UniformCollectionVk*>(pUniformCollection);
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