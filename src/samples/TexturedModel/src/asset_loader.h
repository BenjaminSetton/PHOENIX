#pragma once

#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"
#include "../../common/src/texture_type.h"
#include "../../common/src/serializer.h"
#include "../../common/src/asset_file_format.h"

struct AssetVertex
{
	PHX::Vec3f position;
	PHX::Vec3f normal;
	PHX::Vec3f tangent;
	PHX::Vec3f bitangent;
	PHX::Vec2f uv;
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