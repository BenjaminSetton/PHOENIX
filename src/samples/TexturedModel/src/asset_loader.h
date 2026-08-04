#pragma once

#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"
#include "../../common/src/texture_type.h"
#include "../../common/src/serializer.h"
#include "../../common/src/asset_file_format.h"

struct AssetVertex
{
	BSL::Vec3f position;
	BSL::Vec3f normal;
	BSL::Vec3f tangent;
	BSL::Vec3f bitangent;
	BSL::Vec2f uv;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
	std::vector<Common::TextureType> textures;
};

using AssetManager = Common::AssetManager<AssetType>;

namespace Common
{
	template<>
	struct Serializer<AssetType>
	{
		static constexpr uint32_t TypeHash = HashTypeName("TexturedModel::AssetType");

		static void Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir);
		static AssetType* Read(std::istream& is, const std::filesystem::path& inputDir);
		static AssetType* FromDisk(const AssetDisk& disk);
	};
}