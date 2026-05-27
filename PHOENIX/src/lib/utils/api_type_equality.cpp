
#include "PHX/types/pipeline_desc.h"

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

	static bool CanHandlesBeUsedForComparison(Handle A, Handle B)
	{
		return !(A.IsValid() ^ B.IsValid());
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreHandlesValid(Handle A, Handle B)
	{
		return (A.IsValid() && B.IsValid());
	}

	////////////////////////////////////////////////////////////////////////////////

	// Returns true if both shaders are null, or if both shaders are not null but their
	// contents are equal. Returns false in all other cases
	static bool AreShadersEqual(const IShader* pShaderA, const IShader* pShaderB)
	{
		if (!CanPointersBeUsedForComparison(pShaderA, pShaderB))
		{
			return false;
		}

		bool shadersEqual = true;
		if (ArePointersNotNull(pShaderA, pShaderB))
		{
			shadersEqual = (pShaderA->GetStage() == pShaderB->GetStage());
		}

		return shadersEqual;
	}

	////////////////////////////////////////////////////////////////////////////////

	static bool AreUniformsEqual(UniformCollectionHandle uniformsA, UniformCollectionHandle uniformsB)
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

	bool UniformData::operator==(const UniformData& other) const
	{
		return (binding     == other.binding     &&
				type        == other.type        &&
				shaderStage == other.shaderStage
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
		// SHADERS
		bool shadersEqual = AreShadersEqual(pShader, other.pShader);
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
		// SHADERS
		if (shaderCount != other.shaderCount)
		{
			return false;
		}

		for (u32 i = 0; i < shaderCount; i++)
		{
			const IShader* pShaderA = ppShaders[i];
			const IShader* pShaderB = other.ppShaders[i];
			if (!CanPointersBeUsedForComparison(pShaderA, pShaderB))
			{
				return false;
			}

			if (ArePointersNotNull(pShaderA, pShaderB) && !AreShadersEqual(pShaderA, pShaderB))
			{
				return false;
			}
		}

		// UNIFORMS
		if (!CanHandlesBeUsedForComparison(uniformCollection, other.uniformCollection))
		{
			return false;
		}

		if (AreHandlesValid(uniformCollection, other.uniformCollection) && 
			!AreUniformsEqual(uniformCollection, other.uniformCollection))
		{
			return false;
		}

		// OTHER MEMBERS (compared thru memcmp excluding pointers, which are compared above)
		GraphicsPipelineDesc copyA = *this;
		GraphicsPipelineDesc copyB = other;

		copyA.uniformCollection = INVALID_HANDLE;
		copyA.ppShaders = nullptr;
		copyA.shaderCount = 0;

		copyB.uniformCollection = INVALID_HANDLE;
		copyB.ppShaders = nullptr;
		copyB.shaderCount = 0;

		const int cmpResult = memcmp(&copyA, &copyB, sizeof(GraphicsPipelineDesc));
		return (cmpResult == 0);
	}

	////////////////////////////////////////////////////////////////////////////////
}