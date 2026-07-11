
#include <vulkan/vk_enum_string_helper.h>

#include "acceleration_structure_vk.h"

#include "buffer_vk.h"
#include "render_device_vk.h"
#include "platform/vulkan/utils/acceleration_structure_utils.h"
#include "utils/buffer_utils.h"
#include "utils/logger.h"
#include "utils/sanity.h"

namespace PHX
{
	AccelerationStructureVk::AccelerationStructureVk(RenderDeviceVk* pRenderDevice, const AccelerationStructureCreateInfo& createInfo) :
		m_pRenderDevice(nullptr), m_type(createInfo.type), m_buildFlags(createInfo.buildFlags), m_maxInstanceCount(0), m_accelerationStructure(VK_NULL_HANDLE), m_resultBuffer{}, m_scratchBuffer{}, m_built(false)
	{
		if (pRenderDevice == nullptr)
		{
			LogError("Failed to create acceleration structure. Render device is null!");
			return;
		}

		if (!pRenderDevice->IsRayTracingSupported())
		{
			LogError("Failed to create acceleration structure. Ray tracing is not supported on this device!");
			return;
		}

		if (createInfo.type == ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL)
		{
			if (createInfo.pGeometries == nullptr || createInfo.geometryCount == 0)
			{
				LogError("Failed to create bottom-level acceleration structure. No geometry data provided!");
				return;
			}

			m_geometries.reserve(createInfo.geometryCount);
			for (u32 i = 0; i < createInfo.geometryCount; i++)
			{
				m_geometries.push_back(createInfo.pGeometries[i]);
			}
		}
		else if (createInfo.type == ACCELERATION_STRUCTURE_TYPE::TOP_LEVEL)
		{
			if (createInfo.maxInstanceCount == 0)
			{
				LogError("Failed to create top-level acceleration structure. Max instance count is 0!");
				return;
			}
			m_maxInstanceCount = createInfo.maxInstanceCount;
		}
		else
		{
			LogError("Failed to create acceleration structure. Unknown type!");
			return;
		}

		STATUS_CODE res = Create(pRenderDevice);
		if (res != STATUS_CODE::SUCCESS)
		{
			return;
		}

		m_pRenderDevice = pRenderDevice;
	}

	AccelerationStructureVk::~AccelerationStructureVk()
	{
		Delete();
	}

	ACCELERATION_STRUCTURE_TYPE AccelerationStructureVk::GetType() const
	{
		return m_type;
	}

	AccelerationStructureBuildFlags AccelerationStructureVk::GetBuildFlags() const
	{
		return m_buildFlags;
	}

	bool AccelerationStructureVk::IsBuilt() const
	{
		return m_built;
	}

	VkAccelerationStructureKHR AccelerationStructureVk::GetAccelerationStructure() const
	{
		return m_accelerationStructure;
	}

	VkDeviceAddress AccelerationStructureVk::GetDeviceAddress() const
	{
		if (m_pRenderDevice == nullptr || m_accelerationStructure == VK_NULL_HANDLE)
		{
			return 0;
		}

		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
		addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		addressInfo.accelerationStructure = m_accelerationStructure;
		return m_pRenderDevice->GetAccelerationStructureDeviceAddressKHR(&addressInfo);
	}

	const GeometryData* AccelerationStructureVk::GetGeometries() const
	{
		return m_geometries.data();
	}

	u32 AccelerationStructureVk::GetGeometryCount() const
	{
		return static_cast<u32>(m_geometries.size());
	}

	u32 AccelerationStructureVk::GetMaxInstanceCount() const
	{
		return m_maxInstanceCount;
	}

	BufferData& AccelerationStructureVk::GetScratchBuffer()
	{
		return m_scratchBuffer;
	}

	BufferData& AccelerationStructureVk::GetResultBuffer()
	{
		return m_resultBuffer;
	}

	void AccelerationStructureVk::SetBuilt(bool built)
	{
		m_built = built;
	}

	STATUS_CODE AccelerationStructureVk::Create(RenderDeviceVk* pRenderDevice)
	{
		std::vector<VkAccelerationStructureGeometryKHR> geometries;
		std::vector<u32> maxPrimitiveCounts;

		if (m_type == ACCELERATION_STRUCTURE_TYPE::BOTTOM_LEVEL)
		{
			geometries.reserve(m_geometries.size());
			maxPrimitiveCounts.reserve(m_geometries.size());

			for (const GeometryData& geometry : m_geometries)
			{
				VkAccelerationStructureGeometryKHR asGeometry{};
				asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
				asGeometry.geometryType = ConvertGeometryType(geometry.type);
				asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

				if (geometry.type == GEOMETRY_TYPE::TRIANGLES)
				{
					BufferVk* pVertexBuffer = static_cast<BufferVk*>(pRenderDevice->ResolveHandle(geometry.vertexBuffer));
					BufferVk* pIndexBuffer = static_cast<BufferVk*>(pRenderDevice->ResolveHandle(geometry.indexBuffer));

					if (pVertexBuffer == nullptr)
					{
						LogError("Failed to create acceleration structure. Vertex buffer is invalid!");
						return STATUS_CODE::ERR_API;
					}

					VkDeviceAddress vertexAddress = GetBufferDeviceAddress(pRenderDevice, pVertexBuffer->GetBuffer());
					VkDeviceAddress indexAddress = 0;
					if (pIndexBuffer != nullptr)
					{
						indexAddress = GetBufferDeviceAddress(pRenderDevice, pIndexBuffer->GetBuffer());
					}

					asGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
					asGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
					asGeometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
					asGeometry.geometry.triangles.vertexStride = geometry.vertexStride;
					asGeometry.geometry.triangles.maxVertex = geometry.vertexCount;
					asGeometry.geometry.triangles.indexType = pIndexBuffer != nullptr ? (geometry.indexType == INDEX_TYPE::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32) : VK_INDEX_TYPE_NONE_KHR;
					asGeometry.geometry.triangles.indexData.deviceAddress = indexAddress;
				}
				else if (geometry.type == GEOMETRY_TYPE::AABBS)
				{
					BufferVk* pAABBBuffer = static_cast<BufferVk*>(pRenderDevice->ResolveHandle(geometry.aabbBuffer));
					if (pAABBBuffer == nullptr)
					{
						LogError("Failed to create acceleration structure. AABB buffer is invalid!");
						return STATUS_CODE::ERR_API;
					}

					asGeometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
					asGeometry.geometry.aabbs.data.deviceAddress = GetBufferDeviceAddress(pRenderDevice, pAABBBuffer->GetBuffer());
					asGeometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);
				}

				geometries.push_back(asGeometry);
				maxPrimitiveCounts.push_back(GetMaxPrimitiveCountForGeometry(geometry));
			}
		}
		else if (m_type == ACCELERATION_STRUCTURE_TYPE::TOP_LEVEL)
		{
			VkAccelerationStructureGeometryKHR asGeometry{};
			asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
			asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
			asGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
			asGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
			asGeometry.geometry.instances.data.deviceAddress = 0;
			geometries.push_back(asGeometry);
			maxPrimitiveCounts.push_back(m_maxInstanceCount);
		}

		VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
		buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildGeometryInfo.type = ConvertAccelerationStructureType(m_type);
		buildGeometryInfo.flags = ConvertBuildFlags(m_buildFlags);
		buildGeometryInfo.geometryCount = static_cast<u32>(geometries.size());
		buildGeometryInfo.pGeometries = geometries.data();

		VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
		buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		pRenderDevice->GetAccelerationStructureBuildSizesKHR(VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, maxPrimitiveCounts.data(), &buildSizesInfo);

		const VkBufferUsageFlags resultBufferUsage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_resultBuffer = CreateBuffer(pRenderDevice, buildSizesInfo.accelerationStructureSize, resultBufferUsage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, 0, 0);
		if (!m_resultBuffer.isValid)
		{
			LogError("Failed to create acceleration structure result buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		const VkBufferUsageFlags scratchBufferUsage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		m_scratchBuffer = CreateBuffer(pRenderDevice, buildSizesInfo.buildScratchSize, scratchBufferUsage, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, 0, 0);
		if (!m_scratchBuffer.isValid)
		{
			LogError("Failed to create acceleration structure scratch buffer!");
			return STATUS_CODE::ERR_INTERNAL;
		}

		VkAccelerationStructureCreateInfoKHR asCreateInfo{};
		asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		asCreateInfo.type = ConvertAccelerationStructureType(m_type);
		asCreateInfo.buffer = m_resultBuffer.buffer;
		asCreateInfo.offset = 0;
		asCreateInfo.size = buildSizesInfo.accelerationStructureSize;

		VkResult res = pRenderDevice->CreateAccelerationStructureKHR(&asCreateInfo, &m_accelerationStructure);
		if (res != VK_SUCCESS)
		{
			LogError("Failed to create acceleration structure! Got error: \"%s\"", string_VkResult(res));
			return STATUS_CODE::ERR_INTERNAL;
		}

		return STATUS_CODE::SUCCESS;
	}

	void AccelerationStructureVk::Delete()
	{
		if (m_pRenderDevice != nullptr && m_accelerationStructure != VK_NULL_HANDLE)
		{
			m_pRenderDevice->DestroyAccelerationStructureKHR(m_accelerationStructure);
		}

		if (m_resultBuffer.isValid)
		{
			DestroyBuffer(m_pRenderDevice, m_resultBuffer);
		}

		if (m_scratchBuffer.isValid)
		{
			DestroyBuffer(m_pRenderDevice, m_scratchBuffer);
		}
	}
}
