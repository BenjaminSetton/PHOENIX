#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "core/interface_types/acceleration_structure_interface.h"
#include "PHX/types/acceleration_structure_desc.h"
#include "utils/buffer_utils.h"

namespace PHX
{
	// Forward declarations
	class RenderDeviceVk;

	class AccelerationStructureVk : public IAccelerationStructure
	{
	public:

		AccelerationStructureVk(RenderDeviceVk* pRenderDevice, const AccelerationStructureCreateInfo& createInfo);
		~AccelerationStructureVk() override;

		const char* GetName() const override;
		ACCELERATION_STRUCTURE_TYPE GetType() const override;
		AccelerationStructureBuildFlags GetBuildFlags() const;
		bool IsBuilt() const override;

		VkAccelerationStructureKHR GetAccelerationStructure() const;
		u64 GetDeviceAddress() const override;

		// Bottom-level geometry
		const GeometryData* GetGeometries() const;
		u32 GetGeometryCount() const;

		u32 GetMaxInstanceCount() const;

		// Scratch buffer used for building
		BufferData& GetScratchBuffer();
		BufferData& GetResultBuffer();

		void SetBuilt(bool built);

	private:

		STATUS_CODE Create(RenderDeviceVk* pRenderDevice, const AccelerationStructureCreateInfo& createInfo);
		void Delete();

	private:

		RenderDeviceVk* m_pRenderDevice;

		const char* m_pName;
		ACCELERATION_STRUCTURE_TYPE m_type;
		AccelerationStructureBuildFlags m_buildFlags;

		std::vector<GeometryData> m_geometries;
		u32 m_maxInstanceCount;

		VkAccelerationStructureKHR m_accelerationStructure;

		BufferData m_resultBuffer;
		char* m_resultBufferName;

		BufferData m_scratchBuffer;
		char* m_scratchBufferName;

		bool m_built;
	};
}
