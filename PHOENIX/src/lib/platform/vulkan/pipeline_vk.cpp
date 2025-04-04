
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

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, const GraphicsPipelineCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}

		CreateGraphicsPipeline(pRenderDevice, createInfo);
	}

	PipelineVk::PipelineVk(RenderDeviceVk* pRenderDevice, const ComputePipelineCreateInfo& createInfo)
	{
		if (pRenderDevice == nullptr)
		{
			return;
		}

		CreateComputePipeline(pRenderDevice, createInfo);
	}

	PipelineVk::~PipelineVk()
	{

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

	STATUS_CODE PipelineVk::CreateGraphicsPipeline(RenderDeviceVk* pRenderDevice, const GraphicsPipelineCreateInfo& createInfo)
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

		// Get the associated render pass from the framebuffer
		FramebufferVk* pFramebuffer = dynamic_cast<FramebufferVk*>(createInfo.pFramebuffer);
		VkRenderPass vkRenderPass = RenderPassCache::Get().Find(pFramebuffer->GetRenderPassDescription());
		if (vkRenderPass == VK_NULL_HANDLE)
		{
			LogError("Failed to create pipeline! Render pass associated with framebuffer object is invalid!");
			return STATUS_CODE::ERR_INTERNAL;
		}

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
		pipelineInfo.renderPass = vkRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipelineInfo.basePipelineIndex = -1; // Optional

		VkResult res = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create pipeline! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::CreateComputePipeline(RenderDeviceVk* pRenderDevice, const ComputePipelineCreateInfo& createInfo)
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

		VkResult res = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create compute pipeline! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		m_bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const GraphicsPipelineCreateInfo& createInfo)
	{
		if (createInfo.pInputAttributes == nullptr)
		{
			LogError("Failed to create graphics pipeline! Input attributes are null");
			return STATUS_CODE::ERR_API;
		}
		if (createInfo.pFramebuffer == nullptr)
		{
			LogError("Failed to create graphics pipeline! Framebuffer is null");
			return STATUS_CODE::ERR_API;
		}
		if (createInfo.ppShaders == nullptr)
		{
			LogError("Failed to create graphics pipeline! Shaders array is null");
			return STATUS_CODE::ERR_API;
		}

		return STATUS_CODE::SUCCESS;
	}

	STATUS_CODE PipelineVk::VerifyCreateInfo(const ComputePipelineCreateInfo& createInfo)
	{
		if (createInfo.pShader == nullptr)
		{
			LogError("Failed to create compute pipeline! Shader is null");
			return STATUS_CODE::ERR_API;
		}

		return STATUS_CODE::SUCCESS;
	}

	VkPipelineLayout PipelineVk::CreatePipelineLayout(VkDevice logicalDevice, IUniformCollection* pUniformCollection)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		if (pUniformCollection != nullptr)
		{
			// TODO - Add support for push constants
			UniformCollectionVk* pUniformCollectionVk = dynamic_cast<UniformCollectionVk*>(pUniformCollection);
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