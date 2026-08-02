#include "asset_loader.h"

#include <iostream>

namespace Common
{
	void Serializer<AssetType>::Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir)
	{
		// Write LOD groups
		uint32_t groupCount = static_cast<uint32_t>(asset.lodGroups.size());
		WriteTrivial(os, groupCount);

		for (uint32_t g = 0; g < groupCount; g++)
		{
			const LodGroupData& group = asset.lodGroups[g];
			uint32_t levelCount = static_cast<uint32_t>(group.levels.size());
			WriteTrivial(os, levelCount);

			for (uint32_t l = 0; l < levelCount; l++)
			{
				const LodLevelData& level = group.levels[l];

				WriteTrivial(os, level.screenRatio);

				uint32_t vertexCount = static_cast<uint32_t>(level.vertices.size());
				WriteTrivial(os, vertexCount);
				os.write(reinterpret_cast<const char*>(level.vertices.data()), vertexCount * sizeof(LodVertex));

				uint32_t indexCount = static_cast<uint32_t>(level.indices.size());
				WriteTrivial(os, indexCount);
				os.write(reinterpret_cast<const char*>(level.indices.data()), indexCount * sizeof(Common::AssetIndexType));
			}
		}
	}

	AssetType* Serializer<AssetType>::Read(std::istream& is, const std::filesystem::path& inputDir)
	{
		AssetType* asset = new AssetType();

		uint32_t groupCount = ReadTrivial<uint32_t>(is);
		asset->lodGroups.resize(groupCount);

		for (uint32_t g = 0; g < groupCount; g++)
		{
			LodGroupData& group = asset->lodGroups[g];
			uint32_t levelCount = ReadTrivial<uint32_t>(is);
			group.levels.resize(levelCount);

			for (uint32_t l = 0; l < levelCount; l++)
			{
				LodLevelData& level = group.levels[l];

				level.screenRatio = ReadTrivial<float>(is);

				uint32_t vertexCount = ReadTrivial<uint32_t>(is);
				level.vertices.resize(vertexCount);
				is.read(reinterpret_cast<char*>(level.vertices.data()), vertexCount * sizeof(LodVertex));

				uint32_t indexCount = ReadTrivial<uint32_t>(is);
				level.indices.resize(indexCount);
				is.read(reinterpret_cast<char*>(level.indices.data()), indexCount * sizeof(Common::AssetIndexType));
			}
		}

		return asset;
	}

	AssetType* Serializer<AssetType>::FromDisk(const AssetDisk& disk)
	{
		AssetType* asset = new AssetType();

		// If the asset has authored LOD groups, use them
		if (!disk.lodGroups.empty())
		{
			asset->lodGroups.resize(disk.lodGroups.size());
			for (uint32_t g = 0; g < disk.lodGroups.size(); g++)
			{
				const AssetDiskLodGroup& srcGroup = disk.lodGroups[g];
				LodGroupData& dstGroup = asset->lodGroups[g];
				dstGroup.levels.resize(srcGroup.levels.size());

				for (uint32_t l = 0; l < srcGroup.levels.size(); l++)
				{
					const AssetDiskLodLevel& srcLevel = srcGroup.levels[l];
					LodLevelData& dstLevel = dstGroup.levels[l];

					dstLevel.screenRatio = srcLevel.screenRatio;

					dstLevel.vertices.resize(srcLevel.vertices.size());
					for (uint32_t v = 0; v < srcLevel.vertices.size(); v++)
					{
						dstLevel.vertices[v].position = glm::vec3(srcLevel.vertices[v].position.GetX(), srcLevel.vertices[v].position.GetY(), srcLevel.vertices[v].position.GetZ());
						dstLevel.vertices[v].normal = glm::vec3(srcLevel.vertices[v].normal.GetX(), srcLevel.vertices[v].normal.GetY(), srcLevel.vertices[v].normal.GetZ());
						dstLevel.vertices[v].uv = glm::vec2(srcLevel.vertices[v].uv.GetX(), srcLevel.vertices[v].uv.GetY());
					}

					dstLevel.indices = srcLevel.indices;
				}
			}
		}
		else
		{
			// No authored LODs — create a single LOD group with one level from the base mesh
			LodGroupData group;
			LodLevelData level;
			level.screenRatio = 1.0f;

			level.vertices.resize(disk.vertices.size());
			for (uint32_t v = 0; v < disk.vertices.size(); v++)
			{
				level.vertices[v].position = glm::vec3(disk.vertices[v].position.GetX(), disk.vertices[v].position.GetY(), disk.vertices[v].position.GetZ());
				level.vertices[v].normal = glm::vec3(disk.vertices[v].normal.GetX(), disk.vertices[v].normal.GetY(), disk.vertices[v].normal.GetZ());
				level.vertices[v].uv = glm::vec2(disk.vertices[v].uv.GetX(), disk.vertices[v].uv.GetY());
			}

			level.indices = disk.indices;
			group.levels.push_back(std::move(level));
			asset->lodGroups.push_back(std::move(group));
		}

		std::cout << "[LOD] FromDisk: " << asset->lodGroups.size() << " groups" << std::endl;
		return asset;
	}
}
