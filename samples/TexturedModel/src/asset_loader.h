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

struct TextureMipLevel
{
	void* data			= nullptr;
	PHX::Vec2u size		= {};
	PHX::u64 dataSize; // Represents bytesPerPixel for uncompressed textures (e.g. RGBA8) and blockSize for compressed (e.g. BC1)
};

struct Texture
{
	const char* pName						= "UnnamedTexture";
	Common::TEXTURE_TYPE type				= Common::TEXTURE_TYPE::MAX;
	PHX::BASE_FORMAT format					= PHX::BASE_FORMAT::INVALID;
	std::vector<TextureMipLevel> mipLevels	= {};

	bool IsCompressed() const { return format != PHX::BASE_FORMAT::INVALID; }
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
		newTex.pName = diskTex.pName;
		newTex.type = diskTex.type;
		newTex.format = diskTex.format;

		if (!diskTex.mipLevels.empty())
		{
			// Compressed texture - copy mip chain
			newTex.mipLevels.resize(diskTex.mipLevels.size());
			for (size_t mip = 0; mip < diskTex.mipLevels.size(); mip++)
			{
				newTex.mipLevels[mip].data = diskTex.mipLevels[mip].pData;
				newTex.mipLevels[mip].size = { diskTex.mipLevels[mip].width, diskTex.mipLevels[mip].height };
				newTex.mipLevels[mip].dataSize = diskTex.mipLevels[mip].dataSize;
			}
		}
		else
		{
			// Uncompressed texture - single mip level from flat data
			TextureMipLevel mip{};
			mip.data = diskTex.pData;
			mip.size = diskTex.size;
			mip.dataSize = static_cast<PHX::u64>(diskTex.size.GetX()) * diskTex.size.GetY() * diskTex.bytesPerPixel;
			newTex.mipLevels.push_back(mip);
		}

		newAsset.textures.push_back(newTex);
	}

	Common::AssetHandle id = AssetManager::Get().AddAsset(newAsset);

	return id;
}