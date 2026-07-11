#pragma once

#include "core/ref.h"
#include "PHX/types/acceleration_structure_desc.h"

namespace PHX
{
	class IAccelerationStructure : public RefCounted
	{
	public:

		virtual ~IAccelerationStructure() { }

		virtual ACCELERATION_STRUCTURE_TYPE GetType() const = 0;
		virtual bool IsBuilt() const = 0;
	};
}
