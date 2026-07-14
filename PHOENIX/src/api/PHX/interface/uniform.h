#pragma once

#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/texture.h"
#include "PHX/types/integral_types.h"
#include "PHX/types/status_code.h"
#include "PHX/types/uniform_desc.h"

#include "PHX/interface/handle.h"

namespace PHX
{
	struct UniformCollectionCreateInfo
	{
		UniformDataGroup* dataGroups;
		u32 groupCount;
	};

	struct UniformCollectionHandle : public Handle
	{
		DECLARE_PHX_HANDLE(UniformCollectionHandle);

		u32 GetGroupCount() const;
		const UniformDataGroup* GetGroup(u32 groupIndex) const;
		UniformDataGroup* GetGroup(u32 groupIndex);

		// Queue a buffer update. A size of U64_MAX is used to indicate a "whole buffer" update
		STATUS_CODE QueueBufferUpdate(BufferHandle buffer, u32 set, u32 binding, u64 offset, u64 size = U64_MAX);
		STATUS_CODE QueueImageUpdate(TextureHandle texture, u32 set, u32 binding, u32 imageViewIndex, u32 arrayElement = 0);
		STATUS_CODE QueueAccelerationStructureUpdate(AccelerationStructureHandle accelerationStructure, u32 set, u32 binding);
	};
}