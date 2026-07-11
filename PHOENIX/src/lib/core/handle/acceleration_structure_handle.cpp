
#include "PHX/interface/acceleration_structure.h"

#include "core/handle/handle_utils.h"
#include "core/interface_types/acceleration_structure_interface.h"

namespace PHX
{
	DEFINE_PHX_HANDLE(AccelerationStructureHandle, HANDLE_TYPE::ACCELERATION_STRUCTURE)

	ACCELERATION_STRUCTURE_TYPE AccelerationStructureHandle::GetType() const
	{
		IAccelerationStructure* pAS = static_cast<IAccelerationStructure*>(HANDLE_UTILS::ResolveHandle(*this));
		if (pAS != nullptr)
		{
			return pAS->GetType();
		}

		return ACCELERATION_STRUCTURE_TYPE::MAX;
	}

	bool AccelerationStructureHandle::IsBuilt() const
	{
		IAccelerationStructure* pAS = static_cast<IAccelerationStructure*>(HANDLE_UTILS::ResolveHandle(*this));
		if (pAS != nullptr)
		{
			return pAS->IsBuilt();
		}

		return false;
	}
}
