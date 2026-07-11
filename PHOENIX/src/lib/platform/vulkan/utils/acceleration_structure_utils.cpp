
#include "acceleration_structure_utils.h"

#include "platform/vulkan/render_device_vk.h"
#include "utils/sanity.h"

namespace PHX
{
	VkAccelerationStructureTypeKHR ConvertAccelerationStructureType(ACCELERATION_STRUCTURE_TYPE type)
	{
		switch (type)
		{
		case ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL: return VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		case ACCELERATION_STRUCTURE_TYPE::TOP_LEVEL:    return VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		default:
			ASSERT_ALWAYS("Unknown acceleration structure type!");
			return VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
		}
	}

	VkGeometryTypeKHR ConvertGeometryType(GEOMETRY_TYPE type)
	{
		switch (type)
		{
		case GEOMETRY_TYPE::TRIANGLES: return VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		case GEOMETRY_TYPE::AABBS:     return VK_GEOMETRY_TYPE_AABBS_KHR;
		default:
			ASSERT_ALWAYS("Unknown geometry type!");
			return VK_GEOMETRY_TYPE_MAX_ENUM_KHR;
		}
	}

	VkBuildAccelerationStructureFlagsKHR ConvertBuildFlags(AccelerationStructureBuildFlags flags)
	{
		VkBuildAccelerationStructureFlagsKHR vkFlags = 0;

		if (flags & AS_FLAG_ALLOW_UPDATE)      vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		if (flags & AS_FLAG_ALLOW_COMPACTION)  vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
		if (flags & AS_FLAG_PREFER_FAST_TRACE) vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		if (flags & AS_FLAG_PREFER_FAST_BUILD) vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
		if (flags & AS_FLAG_LOW_MEMORY)        vkFlags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;

		return vkFlags;
	}

	u32 GetMaxPrimitiveCountForGeometry(const GeometryData& geometry)
	{
		if (geometry.type == GEOMETRY_TYPE::TRIANGLES)
		{
			if (geometry.indexBuffer.IsValid() && geometry.indexCount > 0)
			{
				return geometry.indexCount / 3;
			}
			return geometry.vertexCount / 3;
		}
		else if (geometry.type == GEOMETRY_TYPE::AABBS)
		{
			return geometry.aabbCount;
		}

		return 0;
	}

	VkDeviceAddress GetBufferDeviceAddress(RenderDeviceVk* pRenderDevice, VkBuffer buffer)
	{
		VkBufferDeviceAddressInfo addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		addressInfo.buffer = buffer;
		return pRenderDevice->GetBufferDeviceAddressKHR(&addressInfo);
	}
}