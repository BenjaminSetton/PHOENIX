#pragma once

#include <glm.hpp>
#include <string>
#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"
#include "../../common/src/texture_type.h"
#include "../../common/src/serializer.h"
#include "../../common/src/asset_file_format.h"

struct LodVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct LodLevelData
{
	std::vector<LodVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
	float screenRatio = 1.0f;
};

struct LodGroupData
{
	std::vector<LodLevelData> levels;
};

struct AssetType
{
	std::vector<LodGroupData> lodGroups;
};

using AssetManager = Common::AssetManager<AssetType>;

namespace Common
{
	template<>
	struct Serializer<AssetType>
	{
		static constexpr uint32_t TypeHash = HashTypeName("Lod::AssetType");

		static void Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir);
		static AssetType* Read(std::istream& is, const std::filesystem::path& inputDir);
		static AssetType* FromDisk(const AssetDisk& disk);
	};
}
