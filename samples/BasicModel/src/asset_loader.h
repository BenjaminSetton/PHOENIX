#pragma once

#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"

struct AssetVertex
{
	PHX::Vec3f position;
	PHX::Vec3f normal;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
};

using AssetManager = Common::GenericAssetManager<AssetType>;

static Common::AssetHandle ConvertAssetDiskToAssetType(const Common::AssetDisk* pAssetDisk)
{
	AssetType newAsset;
	const uint32_t vertexCount = static_cast<uint32_t>(pAssetDisk->vertices.size());

	// VERTICES
	for (uint32_t i = 0; i < vertexCount; i++)
	{
		const Common::AssetDiskVertex& diskVert = pAssetDisk->vertices[i];
		AssetVertex newVert;
		newVert.position = diskVert.position;
		newVert.normal = diskVert.normal;

		newAsset.vertices.push_back(newVert);
	}

	// INDICES
	newAsset.indices = pAssetDisk->indices;

	Common::AssetHandle id = AssetManager::Get().AddAsset(newAsset);

	return id;
}