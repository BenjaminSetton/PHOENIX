#pragma once

#include "PHX/interface/handle.h"
#include "PHX/types/acceleration_structure_desc.h"

namespace PHX
{
	struct AccelerationStructureHandle : public Handle
	{
		DECLARE_PHX_HANDLE(AccelerationStructureHandle);

		ACCELERATION_STRUCTURE_TYPE GetType() const;
		bool IsBuilt() const;
	};
}
