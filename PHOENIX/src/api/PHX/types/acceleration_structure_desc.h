#pragma once

#include "PHX/types/geometry_data.h"
#include "PHX/types/integral_types.h"

namespace PHX
{
	enum class ACCELERATION_STRUCTURE_TYPE
	{
		BOTTOM_LEVEL = 0,
		TOP_LEVEL,

		MAX
	};

	enum ACCELERATION_STRUCTURE_BUILD_FLAG : u8
	{
		AS_FLAG_NONE              = 0,
		AS_FLAG_ALLOW_UPDATE      = 1 << 0,
		AS_FLAG_ALLOW_COMPACTION  = 1 << 1,
		AS_FLAG_PREFER_FAST_TRACE = 1 << 2,
		AS_FLAG_PREFER_FAST_BUILD = 1 << 3,
		AS_FLAG_LOW_MEMORY        = 1 << 4,
	};
	using AccelerationStructureBuildFlags = u8;

	struct AccelerationStructureCreateInfo
	{
		ACCELERATION_STRUCTURE_TYPE type = ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL;

		// For bottom-level acceleration structures
		GeometryData* pGeometries = nullptr;
		u32 geometryCount = 0;

		// For top-level acceleration structures
		u32 maxInstanceCount = 0;

		AccelerationStructureBuildFlags buildFlags = AS_FLAG_NONE;
	};
}
