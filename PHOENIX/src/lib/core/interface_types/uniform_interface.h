#pragma once

#include "core/ref.h"
#include "PHX/interface/acceleration_structure.h"
#include "PHX/interface/buffer.h"
#include "PHX/interface/texture.h"
#include "PHX/types/status_code.h"
#include "PHX/types/uniform_desc.h"

namespace PHX
{
	// Represents all the uniform data used by a particular pipeline. The uniform data can
	// be split into as many uniform groups as needed, but the uniform data must represent
	// all the possible uniform slots used in the pipeline
	class IUniformCollection : public RefCounted
	{
	public:

		virtual ~IUniformCollection() { }

		virtual u32 GetGroupCount() const = 0;
		virtual const UniformDataGroup* GetGroup(u32 groupIndex) const = 0;
		virtual UniformDataGroup* GetGroup(u32 groupIndex) = 0;

		// Queue a buffer update. A size of U64_MAX is used to indicate a "whole buffer" update
		virtual STATUS_CODE QueueBufferUpdate(BufferHandle buffer, u32 set, u32 binding, u64 offset, u64 size) = 0;
		virtual STATUS_CODE QueueImageUpdate(TextureHandle texture, u32 set, u32 binding, u32 imageViewIndex, u32 arrayElement) = 0;
		virtual STATUS_CODE QueueAccelerationStructureUpdate(AccelerationStructureHandle accelerationStructure, u32 set, u32 binding) = 0;
		virtual STATUS_CODE FlushUpdateQueue() = 0;
	};
}