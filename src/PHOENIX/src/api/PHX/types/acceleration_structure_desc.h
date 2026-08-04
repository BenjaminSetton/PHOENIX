#pragma once

#include "BSL/integral_types.h"
#include "PHX/types/geometry_data.h"

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

	enum ACCELERATION_STRUCTURE_INSTANCE_FLAG : u32
	{
		AS_INSTANCE_FLAG_TRIANGLE_FACING_CULL_DISABLE     = 1 << 0,
		AS_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE  = 1 << 1,
		AS_INSTANCE_FLAG_FORCE_OPAQUE                     = 1 << 2,
		AS_INSTANCE_FLAG_FORCE_NO_OPAQUE                  = 1 << 3,
	};

	struct AccelerationStructureInstance
	{
		float transform[3][4]; // TODO - Replace with mat3 type
		u32 instanceCustomIndex : 24;
		u32 mask : 8;
		u32 instanceShaderBindingTableRecordOffset : 24;
		u32 flags : 8;
		u64 accelerationStructureReference;
	};
}
