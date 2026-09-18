
#include "PHX/types/pipeline_desc.h"

#include "BSL/sanity.h"

namespace PHX
{
	////////////////////////////////////////////////////////////////////////////////
	
	// Returns true if both pointers are either null or not null. The actual
	// pointer values are not important in this case
	static bool CanPointersBeUsedForComparison(const void* pA, const void* pB)
	{
		return !(static_cast<bool>(pA) ^ static_cast<bool>(pB));
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool ArePointersNotNull(const void* pA, const void* pB)
	{
		return (pA != nullptr && pB != nullptr);
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool CanHandlesBeUsedForComparison(const Handle& A, const Handle& B)
	{
		return !(A.IsValid() ^ B.IsValid());
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreHandlesValid(const Handle& A, const Handle& B)
	{
		return (A.IsValid() && B.IsValid());
	}

	////////////////////////////////////////////////////////////////////////////////

	// Returns true if both shaders are null, or if both shaders are not null but their
	// contents are equal. Returns false in all other cases
	static bool AreShadersEqual(const ShaderHandle& A, const ShaderHandle& B)
	{
		if (!CanHandlesBeUsedForComparison(A, B))
		{
			return false;
		}

		bool shadersEqual = true;
		if (AreHandlesValid(A, B))
		{
			shadersEqual = (A.GetStage() == B.GetStage() && A.GetIndex() == B.GetIndex());
		}

		return shadersEqual;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreUniformsEqual(const UniformCollectionHandle& uniformsA, const UniformCollectionHandle& uniformsB)
	{
		if (!CanHandlesBeUsedForComparison(uniformsA, uniformsB))
		{
			return false;
		}

		if (AreHandlesValid(uniformsA, uniformsB))
		{
			const u32 thisGroupCount = uniformsA.GetGroupCount();
			const u32 otherGroupCount = uniformsB.GetGroupCount();
			if (thisGroupCount != otherGroupCount)
			{
				return false;
			}

			for (u32 i = 0; i < thisGroupCount; i++)
			{
				const UniformDataGroup& thisDataGroup = *(uniformsA.GetGroup(i));
				const UniformDataGroup& otherDataGroup = *(uniformsB.GetGroup(i));
				if (!(thisDataGroup == otherDataGroup))
				{
					return false;
				}
			}
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreShaderArraysEqual(const ShaderHandle* pA, const ShaderHandle* pB, u32 count)
	{
		if (!CanPointersBeUsedForComparison(pA, pB))
		{
			return false;
		}

		if (ArePointersNotNull(pA, pB))
		{
			for (u32 i = 0; i < count; i++)
			{
				if (!AreShadersEqual(pA[i], pB[i]))
				{
					return false;
				}
			}
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreInputAttributesEqual(const InputAttribute* pA, const InputAttribute* pB, u32 count)
	{
		if (!CanPointersBeUsedForComparison(pA, pB))
		{
			return false;
		}

		if (ArePointersNotNull(pA, pB))
		{
			for (u32 i = 0; i < count; i++)
			{
				const InputAttribute& attributeA = pA[i];
				const InputAttribute& attributeB = pB[i];
				if (attributeA.location != attributeB.location ||
					attributeA.binding  != attributeB.binding  ||
					attributeA.format   != attributeB.format)
				{
					return false;
				}
			}
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreHitGroupsEqual(const HitGroupDesc* pA, const HitGroupDesc* pB, u32 count)
	{
		if (!CanPointersBeUsedForComparison(pA, pB))
		{
			return false;
		}

		if (ArePointersNotNull(pA, pB))
		{
			for (u32 i = 0; i < count; i++)
			{
				const HitGroupDesc& groupA = pA[i];
				const HitGroupDesc& groupB = pB[i];
				if (groupA.closestHitShaderIndex   != groupB.closestHitShaderIndex   ||
					groupA.anyHitShaderIndex       != groupB.anyHitShaderIndex       ||
					groupA.intersectionShaderIndex != groupB.intersectionShaderIndex)
				{
					return false;
				}
			}
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreStencilOpStatesEqual(const StencilOpState& A, const StencilOpState& B)
	{
		return (A.failOp      == B.failOp      &&
				A.passOp      == B.passOp      &&
				A.depthFailOp == B.depthFailOp &&
				A.compareOp   == B.compareOp   &&
				A.compareMask == B.compareMask &&
				A.writeMask   == B.writeMask   &&
				A.reference   == B.reference);
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreBlendStatesEqual(const BlendState& A, const BlendState& B)
	{
		return (A.enableBlend     == B.enableBlend     &&
				A.srcColorFactor  == B.srcColorFactor  &&
				A.dstColorFactor  == B.dstColorFactor  &&
				A.colorBlendOp    == B.colorBlendOp    &&
				A.srcAlphaFactor  == B.srcAlphaFactor  &&
				A.dstAlphaFactor  == B.dstAlphaFactor  &&
				A.alphaBlendOp    == B.alphaBlendOp    &&
				A.colorWriteMask  == B.colorWriteMask);
	}

	////////////////////////////////////////////////////////////////////////////////

	bool UniformData::operator==(const UniformData& other) const
	{
		return (binding     == other.binding     &&
				type        == other.type        &&
				shaderStageFlags == other.shaderStageFlags
		);
	}

	////////////////////////////////////////////////////////////////////////////////

	bool UniformDataGroup::operator==(const UniformDataGroup& other) const
	{
		if (set != other.set)
		{
			return false;
		}
		
		if (uniformArrayCount != other.uniformArrayCount)
		{
			return false;
		}

		if (!CanPointersBeUsedForComparison(uniformArray, other.uniformArray))
		{
			return false;
		}

		// Compare uniform arrays
		bool uniformArraysEqual = true;
		if (ArePointersNotNull(uniformArray, other.uniformArray))
		{
			for (u32 i = 0; i < uniformArrayCount; i++)
			{
				const UniformData& thisData = uniformArray[i];
				const UniformData& otherData = other.uniformArray[i];
				uniformArraysEqual &= (thisData == otherData);
			}
		}

		return uniformArraysEqual;
	}

	////////////////////////////////////////////////////////////////////////////////

	bool ComputePipelineDesc::operator==(const ComputePipelineDesc& other) const
	{
		STATIC_ASSERT_MSG(sizeof(ComputePipelineDesc) == 32, "If compute pipeline description changed, make sure to change this equality function!");

		// SHADERS
		bool shadersEqual = AreShadersEqual(shader, other.shader);
		if (!shadersEqual)
		{
			return false;
		}

		// UNIFORMS
		bool uniformsEqual = AreUniformsEqual(uniformCollection, other.uniformCollection);

		// At this point we know shaders are equal because it's been checked, so simply
		// check for uniforms being equal
		return uniformsEqual;
	}

	////////////////////////////////////////////////////////////////////////////////

	bool GraphicsPipelineDesc::operator==(const GraphicsPipelineDesc& other) const
	{
		STATIC_ASSERT_MSG(sizeof(GraphicsPipelineDesc) == 256, "If graphics pipeline description changed, make sure to change this equality function!");

		// Input assembler
		if (topology != other.topology ||
			enableRestartPrimitives != other.enableRestartPrimitives ||
			patchControlPoints != other.patchControlPoints)
		{
			return false;
		}

		// Input attributes
		if (attributeCount != other.attributeCount)
		{
			return false;
		}

		if (!AreInputAttributesEqual(pInputAttributes, other.pInputAttributes, attributeCount))
		{
			return false;
		}

		if (inputBinding != other.inputBinding ||
			inputRate != other.inputRate)
		{
			return false;
		}

		// Viewport info
		if (!(viewportPos == other.viewportPos) ||
			!(viewportSize == other.viewportSize) ||
			!(viewportDepthRange == other.viewportDepthRange))
		{
			return false;
		}

		// Scissor info
		if (!(scissorOffset == other.scissorOffset) ||
			!(scissorExtent == other.scissorExtent))
		{
			return false;
		}

		// Rasterizer state
		if (enableDepthClamp != other.enableDepthClamp ||
			enableRasterizerDiscard != other.enableRasterizerDiscard ||
			polygonMode != other.polygonMode ||
			cullMode != other.cullMode ||
			frontFaceWinding != other.frontFaceWinding ||
			enableDepthBias != other.enableDepthBias ||
			depthBiasConstantFactor != other.depthBiasConstantFactor ||
			depthBiasClamp != other.depthBiasClamp ||
			depthBiasSlopeFactor != other.depthBiasSlopeFactor ||
			lineWidth != other.lineWidth)
		{
			return false;
		}

		// Multi-sampling state
		if (rasterizationSamples != other.rasterizationSamples ||
			enableAlphaToCoverage != other.enableAlphaToCoverage ||
			enableAlphaToOne != other.enableAlphaToOne)
		{
			return false;
		}

		// Depth-stencil state
		if (enableDepthTest != other.enableDepthTest ||
			enableDepthWrite != other.enableDepthWrite ||
			compareOp != other.compareOp ||
			enableDepthBoundsTest != other.enableDepthBoundsTest ||
			enableStencilTest != other.enableStencilTest)
		{
			return false;
		}

		if (!AreStencilOpStatesEqual(stencilFront, other.stencilFront) ||
			!AreStencilOpStatesEqual(stencilBack, other.stencilBack))
		{
			return false;
		}

		if (!(depthBoundsRange == other.depthBoundsRange))
		{
			return false;
		}

		// Color blend state
		if (!AreBlendStatesEqual(blendState, other.blendState))
		{
			return false;
		}

		// UNIFORMS
		if (!AreUniformsEqual(uniformCollection, other.uniformCollection))
		{
			return false;
		}

		// SHADERS
		if (shaderCount != other.shaderCount)
		{
			return false;
		}

		if (!AreShaderArraysEqual(pShaders, other.pShaders, shaderCount))
		{
			return false;
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	bool RayTracingPipelineDesc::operator==(const RayTracingPipelineDesc& other) const
	{
		STATIC_ASSERT_MSG(sizeof(RayTracingPipelineDesc) == 56, "If ray tracing pipeline description changed, make sure to change this equality function!");

		// SHADERS
		if (shaderCount != other.shaderCount)
		{
			return false;
		}

		if (!AreShaderArraysEqual(pShaders, other.pShaders, shaderCount))
		{
			return false;
		}

		// HIT GROUPS
		if (hitGroupCount != other.hitGroupCount)
		{
			return false;
		}

		if (!AreHitGroupsEqual(pHitGroups, other.pHitGroups, hitGroupCount))
		{
			return false;
		}

		// UNIFORMS
		if (!AreUniformsEqual(uniformCollection, other.uniformCollection))
		{
			return false;
		}

		if (maxRecursionDepth != other.maxRecursionDepth)
		{
			return false;
		}

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////
}