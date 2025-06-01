
#include <vulkan/vk_enum_string_helper.h>

#include "pipeline_cache.h"

#include "../render_device_vk.h"
#include "utils/cache_utils.h"
#include "utils/logger.h"

namespace PHX
{
	
	size_t GraphicsPipelineDescHasher::operator()(const GraphicsPipelineDesc& desc) const
	{
		//STATIC_ASSERT_MSG(sizeof(desc) == SOME_SIZE, "If graphics pipeline description changed, make sure to change this hashing function!");

		size_t seed = 0;

		// Input assembler
		HashCombine(seed, desc.topology);
		HashCombine(seed, desc.enableRestartPrimitives);

		// Input attributes
		if (desc.pInputAttributes != nullptr)
		{
			HashCombine(seed, desc.attributeCount);

			for (u32 i = 0; i < desc.attributeCount; i++)
			{
				const InputAttribute& currAttribute = desc.pInputAttributes[i];
				HashCombine(seed, currAttribute.location);
				HashCombine(seed, currAttribute.binding);
				HashCombine(seed, currAttribute.format);
			}
		}
		HashCombine(seed, desc.inputBinding);
		HashCombine(seed, desc.inputRate);

		// Viewport info
		HashCombine(seed, desc.viewportPos);
		HashCombine(seed, desc.viewportSize);
		HashCombine(seed, desc.viewportDepthRange);

		// Scissor info
		HashCombine(seed, desc.scissorOffset);
		HashCombine(seed, desc.scissorExtent);

		// Rasterizer state
		HashCombine(seed, desc.enableDepthClamp);
		HashCombine(seed, desc.enableRasterizerDiscard);
		HashCombine(seed, desc.polygonMode);
		HashCombine(seed, desc.cullMode);
		HashCombine(seed, desc.frontFaceWinding);
		HashCombine(seed, desc.enableDepthBias);
		HashCombine(seed, desc.depthBiasConstantFactor);
		HashCombine(seed, desc.depthBiasClamp);
		HashCombine(seed, desc.depthBiasSlopeFactor);
		HashCombine(seed, desc.lineWidth);

		// Multi-sampling state
		HashCombine(seed, desc.rasterizationSamples);
		HashCombine(seed, desc.enableAlphaToCoverage);
		HashCombine(seed, desc.enableAlphaToOne);

		// Depth-stencil state
		HashCombine(seed, desc.enableDepthTest);
		HashCombine(seed, desc.enableDepthWrite);
		HashCombine(seed, desc.compareOp);
		HashCombine(seed, desc.enableDepthBoundsTest);
		HashCombine(seed, desc.enableStencilTest);
		HashCombine(seed, desc.stencilFront.failOp);
		HashCombine(seed, desc.stencilFront.passOp);
		HashCombine(seed, desc.stencilFront.depthFailOp);
		HashCombine(seed, desc.stencilFront.compareOp);
		HashCombine(seed, desc.stencilFront.compareMask);
		HashCombine(seed, desc.stencilFront.reference);
		HashCombine(seed, desc.stencilBack.failOp);
		HashCombine(seed, desc.stencilBack.passOp);
		HashCombine(seed, desc.stencilBack.depthFailOp);
		HashCombine(seed, desc.stencilBack.compareOp);
		HashCombine(seed, desc.stencilBack.compareMask);
		HashCombine(seed, desc.stencilBack.reference);
		HashCombine(seed, desc.depthBoundsRange);

		// Uniform collection
		if (desc.pUniformCollection != nullptr)
		{
			const u32 uniformGroupCount = desc.pUniformCollection->GetGroupCount();
			HashCombine(seed, uniformGroupCount);

			for (u32 i = 0; i < uniformGroupCount; i++)
			{
				const UniformDataGroup& currUniformGroup = *(desc.pUniformCollection->GetGroup(i));
				const u32 uniformArrayCount = currUniformGroup.uniformArrayCount;

				HashCombine(seed, currUniformGroup.set);

				if (currUniformGroup.uniformArray != nullptr)
				{
					HashCombine(seed, uniformArrayCount);

					for (u32 j = 0; j < uniformArrayCount; j++)
					{
						const UniformData& currUniformData = currUniformGroup.uniformArray[j];
						HashCombine(seed, currUniformData.binding);
						HashCombine(seed, currUniformData.shaderStage);
						HashCombine(seed, currUniformData.type);
					}
				}
			}
		}

		// Shader info
		if (desc.ppShaders != nullptr)
		{
			const u32 shaderCount = desc.shaderCount;
			HashCombine(seed, shaderCount);

			for (u32 i = 0; i < shaderCount; i++)
			{
				IShader* currShader = desc.ppShaders[i];
				if (currShader != nullptr)
				{
					HashCombine(seed, currShader->GetStage());
				}
			}
		}

		return seed;
	}

	size_t ComputePipelineDescHasher::operator()(const ComputePipelineDesc& desc) const
	{
		//STATIC_ASSERT_MSG(sizeof(desc) == SOME_SIZE, "If compute pipeline description changed, make sure to change this hashing function!");

		size_t seed = 0;

		// Shader info
		if (desc.pShader != nullptr)
		{
			HashCombine(seed, desc.pShader->GetStage());
		}

		// Uniform collection
		if (desc.pUniformCollection != nullptr)
		{
			const u32 uniformGroupCount = desc.pUniformCollection->GetGroupCount();
			HashCombine(seed, uniformGroupCount);

			for (u32 i = 0; i < uniformGroupCount; i++)
			{
				const UniformDataGroup& currUniformGroup = *(desc.pUniformCollection->GetGroup(i));
				const u32 uniformArrayCount = currUniformGroup.uniformArrayCount;

				HashCombine(seed, currUniformGroup.set);

				if (currUniformGroup.uniformArray != nullptr)
				{
					HashCombine(seed, uniformArrayCount);

					for (u32 j = 0; j < uniformArrayCount; j++)
					{
						const UniformData& currUniformData = currUniformGroup.uniformArray[j];
						HashCombine(seed, currUniformData.binding);
						HashCombine(seed, currUniformData.shaderStage);
						HashCombine(seed, currUniformData.type);
					}
				}
			}
		}

		return seed;
	}

	PipelineCache::PipelineCache(RenderDeviceVk* pRenderDevice) : m_renderDevice(pRenderDevice)
	{
		VkPipelineCacheCreateInfo cacheCI{};
		cacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cacheCI.pNext = nullptr;
		cacheCI.flags = 0;
		cacheCI.pInitialData = nullptr;
		cacheCI.initialDataSize = 0;

		VkResult res = vkCreatePipelineCache(pRenderDevice->GetLogicalDevice(), &cacheCI, nullptr, &m_vkCache);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create pipeline cache! Got error: \"%s\"", string_VkResult(res));
		}
	}

	PipelineCache::~PipelineCache()
	{
		for (auto iter : m_graphicsPipelineCache)
		{
			delete iter.second;
		}
		m_graphicsPipelineCache.clear();

		for (auto iter : m_computePipelineCache)
		{
			delete iter.second;
		}
		m_graphicsPipelineCache.clear();

		vkDestroyPipelineCache(m_renderDevice->GetLogicalDevice(), m_vkCache, nullptr);
	}

	// GRAPHICS
	PipelineVk* PipelineCache::FindOrCreate(RenderDeviceVk* pRenderDevice, VkRenderPass renderPass, const GraphicsPipelineDesc& desc)
	{
		PipelineVk* res = nullptr;

		auto iter = m_graphicsPipelineCache.find(desc);
		if (iter == m_graphicsPipelineCache.end())
		{
			PipelineVk* newPipeline = new PipelineVk(pRenderDevice, m_vkCache, renderPass, desc);
			m_graphicsPipelineCache.insert({desc, newPipeline});
			res = newPipeline;

			LogDebug("Graphics pipeline added to cache. New cache size: %u", m_graphicsPipelineCache.size());
		}
		else
		{
			res = iter->second;
		}

		return res;
	}

	PipelineVk* PipelineCache::Find(const GraphicsPipelineDesc& desc)
	{
		auto iter = m_graphicsPipelineCache.find(desc);
		if (iter != m_graphicsPipelineCache.end())
		{
			return iter->second;
		}

		return nullptr;
	}

	void PipelineCache::Delete(const GraphicsPipelineDesc& desc)
	{
		m_graphicsPipelineCache.erase(desc);
	}

	// COMPUTE
	PipelineVk* PipelineCache::FindOrCreate(RenderDeviceVk* pRenderDevice, const ComputePipelineDesc& desc)
	{
		PipelineVk* res = nullptr;

		auto iter = m_computePipelineCache.find(desc);
		if (iter == m_computePipelineCache.end())
		{
			PipelineVk* newPipeline = new PipelineVk(pRenderDevice, m_vkCache, desc);
			m_computePipelineCache.insert({ desc, newPipeline });
			res = newPipeline;

			LogDebug("Compute pipeline added to cache. New cache size: %u", m_computePipelineCache.size());
		}
		else
		{
			res = iter->second;
		}

		return res;
	}

	PipelineVk* PipelineCache::Find(const ComputePipelineDesc& desc)
	{
		auto iter = m_computePipelineCache.find(desc);
		if (iter != m_computePipelineCache.end())
		{
			return iter->second;
		}

		return nullptr;
	}

	void PipelineCache::Delete(const ComputePipelineDesc& desc)
	{
		m_computePipelineCache.erase(desc);
	}

}