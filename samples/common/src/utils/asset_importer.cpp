
#include "asset_importer.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>

namespace Common
{
	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath)
	{
		Assimp::Importer importer;

#if defined(FAST_IMPORT)
		uint32_t importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace;
#else
		uint32_t importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_FixInfacingNormals | aiProcess_FindInvalidData;
#endif
		const aiScene* scene = importer.ReadFile(filePath.string(), importFlags);
		if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) == 1 || scene->mRootNode == nullptr)
		{
			std::cout << "[ASSET] Failed to import asset. Got error: \"" << importer.GetErrorString() << "\"" << std::endl;
			return nullptr;
		}

		aiMesh* importedMesh = scene->mMeshes[0];
		if (importedMesh == nullptr)
		{
			return nullptr;
		}

		std::shared_ptr<AssetDisk> asset = std::make_shared<AssetDisk>();
		asset->name = filePath.stem().string();

		// VERTICES
		const uint32_t vertexCount = importedMesh->mNumVertices;
		asset->vertices.reserve(vertexCount);
		for (uint32_t i = 0; i < vertexCount; i++)
		{
			const aiVector3D& importedPos = importedMesh->mVertices[i];
			const aiVector3D& importedNormal = importedMesh->mNormals[i];
			const aiVector3D& importedTangent = importedMesh->mTangents[i];
			const aiVector3D& importedBitangent = importedMesh->mBitangents[i];
			const aiVector3D& importedUVs = importedMesh->HasTextureCoords(0) ? importedMesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);

			AssetDiskVertex vertex{};
			vertex.position = { importedPos.x, importedPos.y, importedPos.z };
			vertex.normal = { importedNormal.x, importedNormal.y, importedNormal.z };
			vertex.tangent = { importedTangent.x, importedTangent.y, importedTangent.z };
			vertex.bitangent = { importedBitangent.x, importedBitangent.y, importedBitangent.z };
			vertex.uv = { importedUVs.x, importedUVs.y };

			asset->vertices.push_back(vertex);
		}

		// INDICES
		const uint32_t faceCount = importedMesh->mNumFaces;
		asset->indices.reserve(faceCount * 3);
		for (uint32_t j = 0; j < faceCount; j++)
		{
			uint32_t indexCount = j * 3;
			const aiFace& importedFace = importedMesh->mFaces[j];

			asset->indices.push_back(importedFace.mIndices[0]);
			asset->indices.push_back(importedFace.mIndices[1]);
			asset->indices.push_back(importedFace.mIndices[2]);
		}

		// TEXTURES (loading standalone for now)
		const uint32_t numTextures = scene->mNumTextures;
		asset->textures.reserve(numTextures);
		for (uint32_t i = 0; i < numTextures; i++)
		{
			aiTexture* importedTexture = scene->mTextures[i];

			AssetDiskTexture newTexture;
			newTexture.size = { static_cast<uint32_t>(importedTexture->mWidth), static_cast<uint32_t>(importedTexture->mHeight) };

			// Populate the texture data. Note from the assimp implementation:
			// The format of the data from the imported texture is always ARGB8888, meaning it's 32-bit aligned
			uint32_t texelSize = static_cast<uint32_t>(newTexture.size.GetX()) * static_cast<uint32_t>(newTexture.size.GetY());
			uint64_t numBytes = texelSize * 4;
			char* data = new char[numBytes];
			memcpy(data, importedTexture->pcData, numBytes);
			newTexture.pData = data;
		}

		return asset;
	}
}