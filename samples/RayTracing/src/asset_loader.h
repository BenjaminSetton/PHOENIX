#pragma once

#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"

struct AssetVertex
{
	PHX::Vec3f position;
	float _pad0;
	PHX::Vec3f normal;
	float _pad1;
	PHX::Vec3f tangent;
	float _pad2;
	PHX::Vec3f bitangent;
	float _pad3;
	PHX::Vec2f uv;
	float _pad4[2];
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

struct Mesh
{
	PHX::u32 firstVertex = 0;
	PHX::u32 vertexCount = 0;
	PHX::u32 firstIndex = 0;
	PHX::u32 indexCount = 0;
	PHX::u32 materialIndex = 0;
};

struct Material
{
	std::string name;
	std::vector<PHX::u32> textureIndices;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
	std::vector<Texture> textures;
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
};

using AssetManager = Common::GenericAssetManager<AssetType>;

Common::AssetHandle ConvertAssetDiskToAssetType(const Common::AssetDisk* pAssetDisk);