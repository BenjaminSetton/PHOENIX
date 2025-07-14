#pragma once

#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"

struct AssetVertex
{
	PHX::Vec3f position;
	PHX::Vec3f normal;
	PHX::Vec3f tangent;
	PHX::Vec3f bitangent;
	PHX::Vec2f uv;
};

struct Texture
{
	void* data;
	PHX::Vec2u size;
	Common::TEXTURE_TYPE type;
	PHX::u32 bytesPerPixel;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;

	std::vector<Texture> textures;
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
		newVert.tangent = diskVert.tangent;
		newVert.bitangent = diskVert.bitangent;
		newVert.uv = diskVert.uv;

		newAsset.vertices.push_back(newVert);
	}

	// INDICES
	newAsset.indices = pAssetDisk->indices;

	// TEXTURES
	for (uint32_t i = 0; i < pAssetDisk->textures.size(); i++)
	{
		const Common::AssetDiskTexture& diskTex = pAssetDisk->textures[i];

		Texture newTex;
		newTex.data = diskTex.pData; // Move the pointer over, no copying
		newTex.size = diskTex.size;
		newTex.type = diskTex.type;
		newTex.bytesPerPixel = diskTex.bytesPerPixel;

		newAsset.textures.push_back(newTex);
	}

	Common::AssetHandle id = AssetManager::Get().AddAsset(newAsset);

	return id;
}