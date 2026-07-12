
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
		newTex.data = diskTex.pData; // Move the pointer over, no copying
		newTex.size = diskTex.size;
		newTex.type = diskTex.type;
		newTex.bytesPerPixel = diskTex.bytesPerPixel;

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