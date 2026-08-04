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
	float _pad0;
	BSL::Vec3f normal;
	float _pad1;
	BSL::Vec3f tangent;
	float _pad2;
	BSL::Vec3f bitangent;
	float _pad3;
	BSL::Vec2f uv;
	float _pad4[2];
};

struct Mesh
{
	u32 firstVertex = 0;
	u32 vertexCount = 0;
	u32 firstIndex = 0;
	u32 indexCount = 0;
	u32 materialIndex = 0;
};

struct Material
{
	std::string name;
	std::vector<u32> textureIndices;
};

struct MaterialInfo
{
	u32 albedoTexIndex    = 0; // DefaultAlbedo
	u32 normalTexIndex    = 1; // DefaultNormal
	u32 specularTexIndex  = 2; // DefaultSpecular
	u32 padding           = 0;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
	std::vector<Common::TextureType> textures;
	std::vector<Mesh> meshes;
	std::vector<Material> materials;
};

using AssetManager = Common::AssetManager<AssetType>;

namespace Common
{
	template<>
	struct Serializer<AssetType>
	{
		static constexpr uint32_t TypeHash = HashTypeName("RayTracing::AssetType");

		static void Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir);
		static AssetType* Read(std::istream& is, const std::filesystem::path& inputDir);
		static AssetType* FromDisk(const AssetDisk& disk);
	};
}