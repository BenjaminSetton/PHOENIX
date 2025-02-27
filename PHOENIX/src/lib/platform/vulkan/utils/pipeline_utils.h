#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "PHX/types/input_attribute.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/pipeline_desc.h"
#include "PHX/types/vec_types.h"

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
	VkPipelineRasterizationStateCreateInfo  PopulateRasterizerStateCreateInfo(VkCullModeFlags cullMode, VkFrontFace windingOrder, VkPolygonMode polygonMode, float lineWidth, bool enableDepthClamp, bool enableRasterizerDiscard, bool enableDepthBias, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor);
	VkPipelineMultisampleStateCreateInfo    PopulateMultisamplingStateCreateInfo(VkSampleCountFlagBits sampleCount, bool enableAlphaToCoverage, bool enableAlphaToOne);
	VkPipelineColorBlendAttachmentState     PopulateColorBlendAttachment(); // No need for parameters yet...
	VkPipelineColorBlendStateCreateInfo     PopulateColorBlendStateCreateInfo(const VkPipelineColorBlendAttachmentState* attachments, u32 blendAttachmentCount);
	VkPipelineDepthStencilStateCreateInfo   PopulateDepthStencilStateCreateInfo(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp compareOp, VkBool32 depthBoundsTestEnable, Vec2f depthBoundsRange, VkBool32 stencilTestEnable, StencilOpState stencilFront, StencilOpState stencilBack);
	VkPipelineLayoutCreateInfo              PopulatePipelineLayoutCreateInfo(const VkDescriptorSetLayout* setLayouts, u32 setLayoutCount, const VkPushConstantRange* pushConstantRanges, u32 pushConstantCount);
	VkPipelineShaderStageCreateInfo         PopulateShaderCreateInfo(const IShader* pShader);
	VkViewport                              PopulateViewportInfo(Vec2u viewportSize, Vec2f depthRange);
	VkRect2D                                PopulateScissorInfo(Vec2u scissorOffset, Vec2u scissorExtent);

}