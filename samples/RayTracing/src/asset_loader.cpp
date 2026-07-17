
#include "asset_loader.h"

#include "../../common/src/serializer_texture.h"

#include <fstream>
#include <iostream>

namespace Common
{
	void Serializer<AssetType>::Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir)
	{
		// Write vertices
		uint32_t vertexCount = static_cast<uint32_t>(asset.vertices.size());
		WriteTrivial(os, vertexCount);
		os.write(reinterpret_cast<const char*>(asset.vertices.data()), vertexCount * sizeof(AssetVertex));

		// Write indices
		uint32_t indexCount = static_cast<uint32_t>(asset.indices.size());
		WriteTrivial(os, indexCount);
		os.write(reinterpret_cast<const char*>(asset.indices.data()), indexCount * sizeof(Common::AssetIndexType));

		// Write textures as separate .tex files
		uint32_t textureCount = static_cast<uint32_t>(asset.textures.size());
		WriteTrivial(os, textureCount);

		for (uint32_t i = 0; i < textureCount; i++)
		{
			std::string texFileName = "tex_" + std::to_string(i) + ".tex";
			WriteString(os, texFileName);

			std::filesystem::path texPath = outputDir / texFileName;
			std::ofstream texFile(texPath, std::ios::binary);
			if (texFile.is_open())
			{
				Serializer<TextureType>::Write(texFile, asset.textures[i], outputDir);
			}
			else
			{
				std::cout << "[ASSET] Failed to write texture file: '" << texPath << "'" << std::endl;
			}
		}

		// Write meshes
		uint32_t meshCount = static_cast<uint32_t>(asset.meshes.size());
		WriteTrivial(os, meshCount);
		os.write(reinterpret_cast<const char*>(asset.meshes.data()), meshCount * sizeof(Mesh));

		// Write materials
		uint32_t materialCount = static_cast<uint32_t>(asset.materials.size());
		WriteTrivial(os, materialCount);
		for (uint32_t i = 0; i < materialCount; i++)
		{
			WriteString(os, asset.materials[i].name);
			uint32_t texIdxCount = static_cast<uint32_t>(asset.materials[i].textureIndices.size());
			WriteTrivial(os, texIdxCount);
			os.write(reinterpret_cast<const char*>(asset.materials[i].textureIndices.data()), texIdxCount * sizeof(PHX::u32));
		}
	}

	AssetType* Serializer<AssetType>::Read(std::istream& is, const std::filesystem::path& inputDir)
	{
		AssetType* asset = new AssetType{};

		// Read vertices
		uint32_t vertexCount = ReadTrivial<uint32_t>(is);
		asset->vertices.resize(vertexCount);
		is.read(reinterpret_cast<char*>(asset->vertices.data()), vertexCount * sizeof(AssetVertex));

		// Read indices
		uint32_t indexCount = ReadTrivial<uint32_t>(is);
		asset->indices.resize(indexCount);
		is.read(reinterpret_cast<char*>(asset->indices.data()), indexCount * sizeof(Common::AssetIndexType));

		// Read textures from separate .tex files
		uint32_t textureCount = ReadTrivial<uint32_t>(is);
		asset->textures.reserve(textureCount);

		for (uint32_t i = 0; i < textureCount; i++)
		{
			std::string texFileName = ReadString(is);
			std::filesystem::path texPath = inputDir / texFileName;

			std::ifstream texFile(texPath, std::ios::binary);
			if (texFile.is_open())
			{
				TextureType* tex = Serializer<TextureType>::Read(texFile, inputDir);
				if (tex)
				{
					asset->textures.push_back(std::move(*tex));
					delete tex;
				}
				else
				{
					asset->textures.push_back(TextureType{});
				}
			}
			else
			{
				std::cout << "[ASSET] Failed to read texture file: '" << texPath << "'" << std::endl;
				asset->textures.push_back(TextureType{});
			}
		}

		// Read meshes
		uint32_t meshCount = ReadTrivial<uint32_t>(is);
		asset->meshes.resize(meshCount);
		is.read(reinterpret_cast<char*>(asset->meshes.data()), meshCount * sizeof(Mesh));

		// Read materials
		uint32_t materialCount = ReadTrivial<uint32_t>(is);
		asset->materials.resize(materialCount);
		for (uint32_t i = 0; i < materialCount; i++)
		{
			asset->materials[i].name = ReadString(is);
			uint32_t texIdxCount = ReadTrivial<uint32_t>(is);
			asset->materials[i].textureIndices.resize(texIdxCount);
			is.read(reinterpret_cast<char*>(asset->materials[i].textureIndices.data()), texIdxCount * sizeof(PHX::u32));
		}

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
			newVert._pad0 = 0.0f;
			newVert.normal = diskVert.normal;
			newVert._pad1 = 0.0f;
			newVert.tangent = diskVert.tangent;
			newVert._pad2 = 0.0f;
			newVert.bitangent = diskVert.bitangent;
			newVert._pad3 = 0.0f;
			newVert.uv = diskVert.uv;
			newVert._pad4[0] = 0.0f;
			newVert._pad4[1] = 0.0f;

			asset->vertices.push_back(newVert);
		}

		// INDICES
		asset->indices = disk.indices;

		// TEXTURES — convert from AssetDiskTexture to Common::TextureType
		asset->textures.reserve(disk.textures.size());
		for (uint32_t i = 0; i < disk.textures.size(); i++)
		{
			TextureType* tex = Serializer<TextureType>::FromDisk(disk.textures[i]);
			if (tex)
			{
				asset->textures.push_back(std::move(*tex));
				delete tex;
			}
		}

		// MESHES
		asset->meshes.reserve(disk.meshes.size());
		for (const auto& diskMesh : disk.meshes)
		{
			Mesh newMesh{};
			newMesh.firstVertex = diskMesh.firstVertex;
			newMesh.vertexCount = diskMesh.vertexCount;
			newMesh.firstIndex = diskMesh.firstIndex;
			newMesh.indexCount = diskMesh.indexCount;
			newMesh.materialIndex = diskMesh.materialIndex;
			asset->meshes.push_back(newMesh);
		}

		// MATERIALS
		asset->materials.reserve(disk.materials.size());
		for (const auto& diskMaterial : disk.materials)
		{
			Material newMaterial{};
			newMaterial.name = diskMaterial.name;
			newMaterial.textureIndices = diskMaterial.textureIndices;
			asset->materials.push_back(newMaterial);
		}

		return asset;
	}
}