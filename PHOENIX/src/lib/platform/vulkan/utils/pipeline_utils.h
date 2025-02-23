#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/types/input_attribute.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
	// Forward declarations
	class IShader;
	struct InputAttribute;

	VkVertexInputBindingDescription         PopulateInputBindingDescription(InputAttribute* pAttributes, u32 attributeCount, u32 inputBinding, VERTEX_INPUT_RATE inputRate);
	void                                    PopulateInputAttributeDescription(InputAttribute* pAttributes, u32 attributeCount, std::vector<VkVertexInputAttributeDescription>& out_inputAttributeDescriptions);
	VkPipelineVertexInputStateCreateInfo    PopulateVertexInputCreateInfo(const VkVertexInputBindingDescription& inputBindingDescription, const std::vector<VkVertexInputAttributeDescription>& inputAttributeDescriptions);
	
	VkPipelineInputAssemblyStateCreateInfo  PopulateInputAssemblyCreateInfo(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
	VkPipelineDynamicStateCreateInfo        PopulateDynamicStateCreateInfo(const VkDynamicState* dynamicStates, u32 dynamicStateCount);
	VkPipelineViewportStateCreateInfo       PopulateViewportStateCreateInfo(const VkViewport* viewports, u32 viewportCount, const VkRect2D* scissors, u32 scissorCount);
	VkPipelineRasterizationStateCreateInfo  PopulateRasterizerStateCreateInfo(VkCullModeFlags cullMode, VkFrontFace windingOrder, VkPolygonMode polygonMode);
	VkPipelineMultisampleStateCreateInfo    PopulateMultisamplingStateCreateInfo(VkSampleCountFlagBits sampleCount);
	VkPipelineColorBlendAttachmentState     PopulateColorBlendAttachment(); // No need for parameters yet...
	VkPipelineColorBlendStateCreateInfo     PopulateColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState* attachments, u32 blendAttachmentCount);
	VkPipelineDepthStencilStateCreateInfo   PopulateDepthStencilStateCreateInfo(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable);
	VkPipelineLayoutCreateInfo              PopulatePipelineLayoutCreateInfo(const VkDescriptorSetLayout* setLayouts, u32 setLayoutCount, const VkPushConstantRange* pushConstantRanges, u32 pushConstantCount);
	VkPipelineShaderStageCreateInfo         PopulateShaderCreateInfo(const IShader* pShader);
	VkViewport                              PopulateViewportInfo(u32 width, u32 height);
	VkRect2D                                PopulateScissorInfo(u32 viewportWidth, u32 viewportHeight);

}