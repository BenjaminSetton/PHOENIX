
#include "pipeline_vk.h"

#include <vector>

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

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}

		switch (createInfo.type)
		{
		case PIPELINE_TYPE::GRAPHICS:
		{
			CreateGraphicsPipeline(pRenderDevice, createInfo);
			break;
		}
		case PIPELINE_TYPE::COMPUTE:
		{
			CreateComputePipeline(pRenderDevice, createInfo);
			break;
		}
		}
	}

	PipelineVk::~PipelineVk()
	{

	}

	STATUS_CODE PipelineVk::CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo)
	{
		STATUS_CODE createInfoRes = VerifyCreateInfo(createInfo);
		if (createInfoRes != STATUS_CODE::SUCCESS)
		{
			return createInfoRes;
		}

		VkDevice logicalDevice = pRenderDevice->GetLogicalDevice();

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		if (createInfo.pUniformCollection != nullptr)
		{
			// TODO - Add support for push constants
			UniformCollectionVk* pUniformCollection = dynamic_cast<UniformCollectionVk*>(createInfo.pUniformCollection);
			pipelineLayoutInfo = PopulatePipelineLayoutCreateInfo(pUniformCollection->GetDescriptorSetLayouts(), pUniformCollection->GetDescriptorSetLayoutCount(), nullptr, 0);
		}
		else
		{
			pipelineLayoutInfo = PopulatePipelineLayoutCreateInfo(nullptr, 0, nullptr, 0);
		}

		if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &m_layout) != VK_SUCCESS)
		{
			LogError("Failed to create pipeline layout!");
			return STATUS_CODE::ERR;
		}

		// Shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (u32 i = 0; i < createInfo.shaderCount; i++)
		{
			shaderStages.emplace_back(PopulateShaderCreateInfo(createInfo.ppShaders[i]));
		}

		// Vertex input layout
		std::vector<VkVertexInputAttributeDescription> inputAttributeDescs;
		PopulateInputAttributeDescription(createInfo.pInputAttributes, createInfo.attributeCount, inputAttributeDescs);
		VkVertexInputBindingDescription inputBindingDesc = PopulateInputBindingDescription(createInfo.pInputAttributes, createInfo.attributeCount, createInfo.inputBinding, createInfo.inputRate);

		// Core pipeline descriptions
		VkPipelineVertexInputStateCreateInfo		vertexInputInfo      = PopulateVertexInputCreateInfo(inputBindingDesc, inputAttributeDescs);
		VkPipelineInputAssemblyStateCreateInfo		inputAssembly        = PopulateInputAssemblyCreateInfo(PIPELINE_UTILS::ConvertPrimitiveTopology(createInfo.topology), createInfo.enableRestartPrimitives);
		VkViewport									viewport             = PopulateViewportInfo(static_cast<u32>(createInfo.viewportWidth), static_cast<u32>(createInfo.viewportHeight));
		VkRect2D									scissor              = PopulateScissorInfo(static_cast<u32>(createInfo.viewportWidth), static_cast<u32>(createInfo.viewportHeight));
		VkPipelineDynamicStateCreateInfo			dynamicState         = PopulateDynamicStateCreateInfo(nullptr, 0);
		VkPipelineViewportStateCreateInfo			viewportState        = PopulateViewportStateCreateInfo(&viewport, 1, &scissor, 1);
		VkPipelineRasterizationStateCreateInfo		rasterizer           = PopulateRasterizerStateCreateInfo(PIPELINE_UTILS::ConvertCullMode(createInfo.cullMode), PIPELINE_UTILS::ConvertFrontFaceWinding(createInfo.frontFaceWinding), PIPELINE_UTILS::ConvertPolygonMode(createInfo.polygonMode));
		VkPipelineMultisampleStateCreateInfo		multisampling        = PopulateMultisamplingStateCreateInfo(TEX_UTILS::ConvertSampleCount(createInfo.rasterizationSamples));
		VkPipelineColorBlendAttachmentState			colorBlendAttachment = PopulateColorBlendAttachment();
		VkPipelineColorBlendStateCreateInfo			colorBlending        = PopulateColorBlendStateCreateInfo(&colorBlendAttachment, 1);
		VkPipelineDepthStencilStateCreateInfo		depthStencil         = PopulateDepthStencilStateCreateInfo(createInfo.enableDepthTest, createInfo.enableDepthWrite, PIPELINE_UTILS::ConvertCompareOp(createInfo.compareOp), createInfo.enableDepthBoundsTest, createInfo.enableStencilTest);

		// Get the associated render pass from the framebuffer
		FramebufferVk* pFramebuffer = dynamic_cast<FramebufferVk*>(createInfo.pFramebuffer);
		VkRenderPass vkRenderPass = RenderPassCache::Get().Find(pFramebuffer->GetRenderPassDescription());
		if (vkRenderPass == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline! Render pass associated with framebuffer object is invalid!");
			return STATUS_CODE::ERR;
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
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
		pipelineInfo.renderPass = vkRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline))
		{
			LogError("Failed to create pipeline!");
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::CreateComputePipeline(RenderDeviceVk* pRenderDevice, const PipelineCreateInfo& createInfo)
	{
		TODO();

		UNUSED(pRenderDevice);
		UNUSED(createInfo);
		return STATUS_CODE::ERR;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const PipelineCreateInfo& createInfo)
	{
		if (createInfo.pInputAttributes == nullptr)
		{
			LogError("Failed to create pipeline! Input attributes are null");
			return STATUS_CODE::ERR;
		}
		if (createInfo.pFramebuffer == nullptr)
		{
			LogError("Failed to create pipeline! Framebuffer is null");
			return STATUS_CODE::ERR;
		}
		if (createInfo.ppShaders == nullptr)
		{
			LogError("Failed to create pipeline! Shaders array is null");
			return STATUS_CODE::ERR;
		}

		return STATUS_CODE::SUCCESS;
	}
}