
#include "pipeline_utils.h"

#include "../shader_vk.h"
#include "PHX/types/input_attribute.h"
#include "pipeline_type_converter.h"
#include "shader_type_converter.h"
#include "texture_type_converter.h"
#include "texture_utils.h"
#include "utils/logger.h"
#include "utils/sanity.h"

namespace PHX
{
	VkVertexInputBindingDescription PopulateInputBindingDescription(InputAttribute* pAttributes, u32 attributeCount, u32 inputBinding, VERTEX_INPUT_RATE inputRate)
	{
		if (pAttributes == nullptr || attributeCount == 0)
		{
			LogError("Failed to populate input binding description. Attributes array is empty or invalid!");
			return {};
		}

		u32 accumulatedStride = 0;
		for (u32 i = 0; i < attributeCount; i++)
		{
			accumulatedStride += GetBaseFormatSize(pAttributes[i].format);
		}

		VkVertexInputBindingDescription bindingDesc{};
		bindingDesc.binding = inputBinding;
		bindingDesc.stride = accumulatedStride;
		bindingDesc.inputRate = PIPELINE_UTILS::ConvertInputRate(inputRate);
		return bindingDesc;
	}

	void PopulateInputAttributeDescription(InputAttribute* pAttributes, u32 attributeCount, std::vector<VkVertexInputAttributeDescription>& out_inputAttributeDescriptions)
	{
		if (pAttributes == nullptr || attributeCount == 0)
		{
			LogError("Failed to populate input attribute description. Attributes array is empty or invalid!");
			return;
		}

		out_inputAttributeDescriptions.reserve(attributeCount);
		
		u32 accumulatedOffset = 0;
		for (u32 i = 0; i < attributeCount; i++)
		{
			InputAttribute& attrib = pAttributes[i];

			accumulatedOffset += GetBaseFormatSize(attrib.format);

			VkVertexInputAttributeDescription attributeDesc{};
			attributeDesc.location = i;
			attributeDesc.binding = 0; // TODO - Figure out what to do with this
			attributeDesc.format = TEX_UTILS::ConvertBaseFormat(attrib.format);
			attributeDesc.offset = accumulatedOffset;

			out_inputAttributeDescriptions.push_back(attributeDesc);
		}
	}

	VkPipelineVertexInputStateCreateInfo PopulateVertexInputCreateInfo(const VkVertexInputBindingDescription& inputBindingDescription, const std::vector<VkVertexInputAttributeDescription>& inputAttributeDescriptions)
	{
		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.pVertexBindingDescriptions = &(inputBindingDescription); // TODO - Do we need more than 1?
		vertexInput.vertexBindingDescriptionCount = 1;
		vertexInput.pVertexAttributeDescriptions = inputAttributeDescriptions.data();
		vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(inputAttributeDescriptions.size());

		return vertexInput;
	}

	VkPipelineInputAssemblyStateCreateInfo PopulateInputAssemblyCreateInfo(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = topology;
		inputAssembly.primitiveRestartEnable = primitiveRestartEnable;

		return inputAssembly;
	}

	VkPipelineDynamicStateCreateInfo PopulateDynamicStateCreateInfo(const VkDynamicState* dynamicStates, u32 dynamicStateCount)
	{
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = dynamicStateCount;
		dynamicState.pDynamicStates = dynamicStates;

		return dynamicState;
	}

	VkPipelineViewportStateCreateInfo PopulateViewportStateCreateInfo(const VkViewport* viewports, u32 viewportCount, const VkRect2D* scissors, u32 scissorCount)
	{
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.pViewports = viewports;
		viewportState.viewportCount = viewportCount;
		viewportState.pScissors = scissors;
		viewportState.scissorCount = scissorCount;

		return viewportState;
	}

	VkPipelineRasterizationStateCreateInfo PopulateRasterizerStateCreateInfo(VkCullModeFlags cullMode, VkFrontFace windingOrder, VkPolygonMode polygonMode)
	{
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = polygonMode;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = cullMode;
		rasterizer.frontFace = windingOrder;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;	// Optional
		rasterizer.depthBiasClamp = 0.0f;			// Optional
		rasterizer.depthBiasSlopeFactor = 0.0f;		// Optional

		return rasterizer;
	}

	VkPipelineMultisampleStateCreateInfo PopulateMultisamplingStateCreateInfo(VkSampleCountFlagBits sampleCount)
	{
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = sampleCount;
		multisampling.minSampleShading = 1.0f;				// Optional
		multisampling.pSampleMask = nullptr;				// Optional
		multisampling.alphaToCoverageEnable = VK_FALSE;		// Optional
		multisampling.alphaToOneEnable = VK_FALSE;			// Optional

		return multisampling;
	}

	VkPipelineColorBlendAttachmentState PopulateColorBlendAttachment()
	{
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;				// Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;		// Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;	// Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;				// Optional

		return colorBlendAttachment;
	}

	VkPipelineColorBlendStateCreateInfo PopulateColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState* attachments, u32 blendAttachmentCount)
	{
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;				// Optional
		colorBlending.attachmentCount = blendAttachmentCount;
		colorBlending.pAttachments = attachments;
		colorBlending.blendConstants[0] = 0.0f;					// Optional
		colorBlending.blendConstants[1] = 0.0f;					// Optional
		colorBlending.blendConstants[2] = 0.0f;					// Optional
		colorBlending.blendConstants[3] = 0.0f;					// Optional

		return colorBlending;
	}

	VkPipelineDepthStencilStateCreateInfo PopulateDepthStencilStateCreateInfo(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable)
	{
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = depthTestEnable;
		depthStencil.depthWriteEnable = depthWriteEnable;
		depthStencil.depthCompareOp = compareOp;
		depthStencil.depthBoundsTestEnable = depthBoundsTestEnable;
		depthStencil.minDepthBounds = 0.0f;				// Optional
		depthStencil.maxDepthBounds = 1.0f;				// Optional
		depthStencil.stencilTestEnable = stencilTestEnable;
		depthStencil.front = {};						// Optional
		depthStencil.back = {};							// Optional

		return depthStencil;
	}

	VkPipelineLayoutCreateInfo PopulatePipelineLayoutCreateInfo(const VkDescriptorSetLayout* setLayouts, u32 setLayoutCount, const VkPushConstantRange* pushConstantRanges, u32 pushConstantCount)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = setLayoutCount;
		pipelineLayoutInfo.pSetLayouts = setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = pushConstantCount;
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges;

		return pipelineLayoutInfo;
	}

	VkPipelineShaderStageCreateInfo PopulateShaderCreateInfo(const IShader* pShader)
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo{};
		shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageInfo.pName = "main";

		if (pShader != nullptr)
		{
			const ShaderVk* pShaderVk = dynamic_cast<const ShaderVk*>(pShader);

			shaderStageInfo.stage = SHADER_UTILS::ConvertShaderStage(pShaderVk->GetStage());
			shaderStageInfo.module = pShaderVk->GetShaderModule();
		}

		return shaderStageInfo;
	}

	VkViewport PopulateViewportInfo(u32 width, u32 height)
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(width);
		viewport.height = static_cast<float>(height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		return viewport;
	}

	VkRect2D PopulateScissorInfo(u32 viewportWidth, u32 viewportHeight)
	{
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = { viewportWidth, viewportHeight };

		return scissor;
	}
}