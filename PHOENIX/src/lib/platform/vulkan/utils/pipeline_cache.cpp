
#include <vulkan/vk_enum_string_helper.h>

#include "pipeline_cache.h"

#include "../render_device_vk.h"
#include "utils/cache_utils.h"
#include "utils/logger.h"

namespace PHX
{
	static void HashCombineUniformCollection(const UniformCollectionHandle& uniformCollection, size_t& out_seed)
	{
		if (uniformCollection.IsValid())
		{
			const u32 uniformGroupCount = uniformCollection.GetGroupCount();
			HashCombine(out_seed, uniformGroupCount);

			for (u32 i = 0; i < uniformGroupCount; i++)
			{
				const UniformDataGroup& currUniformGroup = *(uniformCollection.GetGroup(i));
				const u32 uniformArrayCount = currUniformGroup.uniformArrayCount;

				HashCombine(out_seed, currUniformGroup.set);

				if (currUniformGroup.uniformArray != nullptr)
				{
					HashCombine(out_seed, uniformArrayCount);

					for (u32 j = 0; j < uniformArrayCount; j++)
					{
						const UniformData& currUniformData = currUniformGroup.uniformArray[j];
						HashCombine(out_seed, currUniformData.binding);
						HashCombine(out_seed, currUniformData.shaderStage);
						HashCombine(out_seed, currUniformData.type);
					}
				}
			}
		}
	}

	static void HashCombineShaderArray(const ShaderHandle* pShaders, u32 shaderCount, size_t& out_seed)
	{
		if (pShaders != nullptr)
		{
			HashCombine(out_seed, shaderCount);

			for (u32 i = 0; i < shaderCount; i++)
			{
				const ShaderHandle& currShader = pShaders[i];
				if (currShader != INVALID_HANDLE)
				{
					HashCombine(out_seed, currShader.GetStage());
				}
			}
		}
	}

	size_t GraphicsPipelineDescHasher::operator()(const GraphicsPipelineDesc& desc) const
	{
		STATIC_ASSERT_MSG(sizeof(desc) == 248, "If graphics pipeline description changed, make sure to change this hashing function!");

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

		// Color blend state
		HashCombine(seed, desc.blendState.enableBlend);
		HashCombine(seed, desc.blendState.srcColorFactor);
		HashCombine(seed, desc.blendState.dstColorFactor);
		HashCombine(seed, desc.blendState.colorBlendOp);
		HashCombine(seed, desc.blendState.srcAlphaFactor);
		HashCombine(seed, desc.blendState.dstAlphaFactor);
		HashCombine(seed, desc.blendState.alphaBlendOp);
		HashCombine(seed, desc.blendState.colorWriteMask);

		// Uniform collection
		HashCombineUniformCollection(desc.uniformCollection, seed);

		// Shader info
		HashCombineShaderArray(desc.pShaders, desc.shaderCount, seed);

		return seed;
	}

	size_t ComputePipelineDescHasher::operator()(const ComputePipelineDesc& desc) const
	{
		STATIC_ASSERT_MSG(sizeof(desc) == 32, "If compute pipeline description changed, make sure to change this hashing function!");

		size_t seed = 0;

		// Shader info
		HashCombineShaderArray(&desc.shader, 1, seed);

		// Uniform collection
		HashCombineUniformCollection(desc.uniformCollection, seed);

		return seed;
	}

	size_t RayTracingPipelineDescHasher::operator()(const RayTracingPipelineDesc& desc) const
	{
		STATIC_ASSERT_MSG(sizeof(desc) == 56, "If ray tracing pipeline description changed, make sure to change this hashing function!");

		size_t seed = 0;

		// Shader info
		HashCombineShaderArray(desc.pShaders, desc.shaderCount, seed);

		// Hit group info
		for (u32 i = 0; i < desc.hitGroupCount; i++)
		{
			HashCombine(seed, desc.pHitGroups[i].closestHitShaderIndex);
			HashCombine(seed, desc.pHitGroups[i].anyHitShaderIndex);
			HashCombine(seed, desc.pHitGroups[i].intersectionShaderIndex);
		}

		// Uniform collection
		HashCombineUniformCollection(desc.uniformCollection, seed);

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
		m_computePipelineCache.clear();

		for (auto iter : m_rayTracingPipelineCache)
		{
			delete iter.second;
		}
		m_rayTracingPipelineCache.clear();

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

	// RAY TRACING
	PipelineVk* PipelineCache::FindOrCreate(RenderDeviceVk* pRenderDevice, const RayTracingPipelineDesc& desc)
	{
		PipelineVk* res = nullptr;

		auto iter = m_rayTracingPipelineCache.find(desc);
		if (iter == m_rayTracingPipelineCache.end())
		{
			PipelineVk* newPipeline = new PipelineVk(pRenderDevice, m_vkCache, desc);
			m_rayTracingPipelineCache.insert({ desc, newPipeline });
			res = newPipeline;

			LogDebug("Ray tracing pipeline added to cache. New cache size: %u", m_rayTracingPipelineCache.size());
		}
		else
		{
			res = iter->second;
		}

		return res;
	}

	PipelineVk* PipelineCache::Find(const RayTracingPipelineDesc& desc)
	{
		auto iter = m_rayTracingPipelineCache.find(desc);
		if (iter != m_rayTracingPipelineCache.end())
		{
			return iter->second;
		}

		return nullptr;
	}

	void PipelineCache::Delete(const RayTracingPipelineDesc& desc)
	{
		m_rayTracingPipelineCache.erase(desc);
	}

}