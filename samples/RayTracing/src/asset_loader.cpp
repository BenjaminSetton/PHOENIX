
#include "asset_loader.h"

Common::AssetHandle ConvertAssetDiskToAssetType(const Common::AssetDisk* pAssetDisk)
{
	AssetType newAsset;
	const uint32_t vertexCount = static_cast<uint32_t>(pAssetDisk->vertices.size());

	// VERTICES
	for (uint32_t i = 0; i < vertexCount; i++)
	{
		const Common::AssetDiskVertex& diskVert = pAssetDisk->vertices[i];
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

	// MESHES
	newAsset.meshes.reserve(pAssetDisk->meshes.size());
	for (const auto& diskMesh : pAssetDisk->meshes)
	{
		Mesh newMesh{};
		newMesh.firstVertex = diskMesh.firstVertex;
		newMesh.vertexCount = diskMesh.vertexCount;
		newMesh.firstIndex = diskMesh.firstIndex;
		newMesh.indexCount = diskMesh.indexCount;
		newMesh.materialIndex = diskMesh.materialIndex;
		newAsset.meshes.push_back(newMesh);
	}

	// MATERIALS
	newAsset.materials.reserve(pAssetDisk->materials.size());
	for (const auto& diskMaterial : pAssetDisk->materials)
	{
		Material newMaterial{};
		newMaterial.name = diskMaterial.name;
		newMaterial.textureIndices = diskMaterial.textureIndices;
		newAsset.materials.push_back(newMaterial);
	}

	return AssetManager::Get().AddAsset(newAsset);
}