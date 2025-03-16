#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/texture.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/shader_desc.h"
#include "PHX/types/status_code.h"
#include "PHX/types/uniform_desc.h"

namespace PHX
{
	// AKA a Vulkan descriptor
	struct UniformData
	{
		u32 binding;
		UNIFORM_TYPE type;
		SHADER_STAGE shaderStage;
	};

	// AKA a Vulkan descriptor set
	struct UniformDataGroup
	{
		u32 set;
		UniformData* uniformArray;
		u32 uniformArrayCount;
	};

	struct UniformCollectionCreateInfo
	{
		UniformDataGroup* dataGroups;
		u32 groupCount;
	};

	// Represents all the uniform data used by a particular pipeline. The uniform data can
	// be split into as many uniform groups as needed, but the uniform data must represent
	// all the possible uniform slots used in the pipeline
	class IUniformCollection
	{
	public:

		virtual ~IUniformCollection() { }

		virtual u32 GetGroupCount() const = 0;
		virtual const UniformDataGroup& GetGroup(u32 groupIndex) const = 0;
		virtual UniformDataGroup& GetGroup(u32 groupIndex) = 0;

		// Updating uniform functions
		virtual STATUS_CODE QueueBufferUpdate(u32 set, u32 binding, u32 offset, IBuffer* pBuffer) = 0;
		virtual STATUS_CODE QueueImageUpdate(u32 set, u32 binding, u32 imageViewIndex, ITexture* pTexture) = 0;
		virtual STATUS_CODE FlushUpdateQueue() = 0;
	};
}