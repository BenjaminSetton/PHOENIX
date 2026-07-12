#pragma once

#include "PHX/interface/handle.h"
#include "PHX/types/acceleration_structure_desc.h"

namespace PHX
{
	struct AccelerationStructureCreateInfo
	{
		const char* pName							= "";
		ACCELERATION_STRUCTURE_TYPE type			= ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL;
		GeometryData* pGeometries					= nullptr; // Bottom-level acceleration structure
		u32 geometryCount							= 0; // Bottom-level acceleration structure
		u32 maxInstanceCount						= 0; // Top-level acceleration structure
		AccelerationStructureBuildFlags buildFlags	= AS_FLAG_NONE;
	};
	
	struct AccelerationStructureHandle : public Handle
	{
		DECLARE_PHX_HANDLE(AccelerationStructureHandle);

		ACCELERATION_STRUCTURE_TYPE GetType() const;
		bool IsBuilt() const;
		u64 GetDeviceAddress() const;
	};
}
