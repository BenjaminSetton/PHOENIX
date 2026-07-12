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

struct Texture
{
	const char* pName;
	void* data;
	PHX::Vec2u size;
	Common::TEXTURE_TYPE type;
	PHX::u32 bytesPerPixel;
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