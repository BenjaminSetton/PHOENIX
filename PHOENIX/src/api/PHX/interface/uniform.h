#pragma once

#include "PHX/interface/buffer.h"
#include "PHX/interface/texture.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"
#include "PHX/types/uniform_desc.h"

namespace PHX
{
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
		virtual const UniformDataGroup* GetGroup(u32 groupIndex) const = 0;
		virtual UniformDataGroup* GetGroup(u32 groupIndex) = 0;

		// Queue a buffer update. A size of U64_MAX is used to indicate a "whole buffer" update
		virtual STATUS_CODE QueueBufferUpdate(IBuffer* pBuffer, u32 set, u32 binding, u64 offset, u64 size = U64_MAX) = 0;
		virtual STATUS_CODE QueueImageUpdate(ITexture* pTexture, u32 set, u32 binding, u32 imageViewIndex) = 0;
		virtual STATUS_CODE FlushUpdateQueue() = 0;
	};
}