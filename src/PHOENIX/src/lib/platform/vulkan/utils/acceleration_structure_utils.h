#pragma once

#include <vulkan/vulkan.h>

#include "PHX/types/acceleration_structure_desc.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	VkAccelerationStructureTypeKHR ConvertAccelerationStructureType(ACCELERATION_STRUCTURE_TYPE type);
	VkGeometryTypeKHR ConvertGeometryType(GEOMETRY_TYPE type);
	VkGeometryFlagsKHR ConvertGeometryFlags(GeometryFlags flags);
	VkBuildAccelerationStructureFlagsKHR ConvertBuildFlags(AccelerationStructureBuildFlags flags);
	u32 GetMaxPrimitiveCountForGeometry(const GeometryData& geometry);
	VkDeviceAddress GetBufferDeviceAddress(RenderDeviceVk* pRenderDevice, VkBuffer buffer);
}