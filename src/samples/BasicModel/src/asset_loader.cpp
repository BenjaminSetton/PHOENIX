#include "asset_loader.h"

namespace Common
{
	void Serializer<AssetType>::Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir)
	{
		(void)outputDir; // No textures for BasicModel

		// Write vertices
		uint32_t vertexCount = static_cast<uint32_t>(asset.vertices.size());
		BSL::WriteTrivial(os, vertexCount);
		os.write(reinterpret_cast<const char*>(asset.vertices.data()), vertexCount * sizeof(AssetVertex));

		// Write indices
		uint32_t indexCount = static_cast<uint32_t>(asset.indices.size());
		BSL::WriteTrivial(os, indexCount);
		os.write(reinterpret_cast<const char*>(asset.indices.data()), indexCount * sizeof(Common::AssetIndexType));
	}

	AssetType* Serializer<AssetType>::Read(std::istream& is, const std::filesystem::path& inputDir)
	{
		(void)inputDir; // No textures for BasicModel

		AssetType* asset = new AssetType{};

		// Read vertices
		uint32_t vertexCount = BSL::ReadTrivial<uint32_t>(is);
		asset->vertices.resize(vertexCount);
		is.read(reinterpret_cast<char*>(asset->vertices.data()), vertexCount * sizeof(AssetVertex));

		// Read indices
		uint32_t indexCount = BSL::ReadTrivial<uint32_t>(is);
		asset->indices.resize(indexCount);
		is.read(reinterpret_cast<char*>(asset->indices.data()), indexCount * sizeof(Common::AssetIndexType));

		return asset;
	}

	AssetType* Serializer<AssetType>::FromDisk(const AssetDisk& disk)
	{
		AssetType* asset = new AssetType{};
		const uint32_t vertexCount = static_cast<uint32_t>(disk.vertices.size());

		// VERTICES
		asset->vertices.reserve(vertexCount);
		for (uint32_t i = 0; i < vertexCount; i++)
		{
			const AssetDiskVertex& diskVert = disk.vertices[i];
			AssetVertex newVert;
			newVert.position = diskVert.position;
			newVert.normal = diskVert.normal;

			asset->vertices.push_back(newVert);
		}

		// INDICES
		asset->indices = disk.indices;

		return asset;
	}
}
