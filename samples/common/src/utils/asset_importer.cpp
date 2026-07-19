
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>

#include "asset_importer.h"
#include "tex_utils.h"

#ifndef COMMON_ASSET_ROOT_DIR
	#define COMMON_ASSET_ROOT_DIR "."
#endif

#ifndef SAMPLE_ASSET_ROOT_DIR
	#define SAMPLE_ASSET_ROOT_DIR "."
#endif

namespace Common
{
	std::filesystem::path FindAssetFile(const std::filesystem::path& relativePath)
	{
		std::filesystem::path commonPath = std::filesystem::path(COMMON_ASSET_ROOT_DIR) / relativePath;
		if (std::filesystem::exists(commonPath))
		{
			return commonPath;
		}

		std::filesystem::path samplePath = std::filesystem::path(SAMPLE_ASSET_ROOT_DIR) / relativePath;
		if (std::filesystem::exists(samplePath))
		{
			return samplePath;
		}

		if (std::filesystem::exists(relativePath))
		{
			return relativePath;
		}

		return {};
	}

	static const std::vector<aiTextureType> SUPPORTED_TEXTURE_TYPES =
	{
		aiTextureType_DIFFUSE,
		aiTextureType_SPECULAR,
		aiTextureType_NORMALS,
		aiTextureType_AMBIENT_OCCLUSION,
		aiTextureType_METALNESS,
		aiTextureType_DIFFUSE_ROUGHNESS,
		aiTextureType_LIGHTMAP
	};

	static const std::unordered_map<aiTextureType, TEXTURE_TYPE> AI_TEXTURE_TO_INTERNAL =
	{
		{ aiTextureType_DIFFUSE				, TEXTURE_TYPE::DIFFUSE				},
		{ aiTextureType_SPECULAR			, TEXTURE_TYPE::SPECULAR			},
		{ aiTextureType_NORMALS				, TEXTURE_TYPE::NORMAL				},
		{ aiTextureType_AMBIENT_OCCLUSION	, TEXTURE_TYPE::AMBIENT_OCCLUSION	},
		{ aiTextureType_METALNESS			, TEXTURE_TYPE::METALLIC			},
		{ aiTextureType_DIFFUSE_ROUGHNESS	, TEXTURE_TYPE::ROUGHNESS			},
		{ aiTextureType_LIGHTMAP			, TEXTURE_TYPE::LIGHTMAP			}
	};

	static const std::unordered_map<Common::TEXTURE_TYPE, std::string> TEXTURE_TYPE_TO_STRING =
	{
		{ TEXTURE_TYPE::DIFFUSE				, "diffuse"				},
		{ TEXTURE_TYPE::SPECULAR			, "specular"			},
		{ TEXTURE_TYPE::NORMAL				, "normal"				},
		{ TEXTURE_TYPE::AMBIENT_OCCLUSION	, "ambient occlusion"	},
		{ TEXTURE_TYPE::METALLIC			, "metallic"			},
		{ TEXTURE_TYPE::ROUGHNESS			, "roughness"			},
		{ TEXTURE_TYPE::LIGHTMAP			, "lightmap"			},
		{ TEXTURE_TYPE::MAX					, "invalid"				},
	};

	AssetDiskTexture AllocateTexture(const char* pName, const void* const srcData, PHX::Vec2u size, PHX::u32 bytesPerPixel, TEXTURE_TYPE type)
	{
		AssetDiskTexture diskTex{};
		if (srcData == nullptr)
		{
			return diskTex;
		}

		const uint32_t nameSize = static_cast<uint32_t>(strlen(pName)) + 1;
		diskTex.pName = new char[nameSize];
		strcpy_s(diskTex.pName, nameSize, pName);

		uint32_t texelSize = size.GetX() * size.GetY();
		uint64_t numBytes = texelSize * bytesPerPixel;
		diskTex.pData = new char[numBytes];
		memcpy(diskTex.pData, srcData, numBytes);

		diskTex.size = size;
		diskTex.type = type;
		diskTex.bytesPerPixel = bytesPerPixel;

		return diskTex;
	}

	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath)
	{
		Assimp::Importer importer;

#if defined(FAST_IMPORT)
		uint32_t importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices;
#else
		uint32_t importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_FindInvalidData | aiProcess_PreTransformVertices;
#endif
		const aiScene* scene = importer.ReadFile(filePath.string(), importFlags);
		if (scene == nullptr || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) == 1 || scene->mRootNode == nullptr)
		{
			std::cout << "[ASSET] Failed to import asset. Got error: \"" << importer.GetErrorString() << "\"" << std::endl;
			return nullptr;
		}

		if (scene->mNumMeshes == 0)
		{
			return nullptr;
		}

		std::shared_ptr<AssetDisk> asset = std::make_shared<AssetDisk>();
		asset->name = filePath.stem().string();

		asset->meshes.reserve(scene->mNumMeshes);
		asset->materials.reserve(scene->mNumMaterials);

		uint32_t totalVertexCount = 0;
		uint32_t totalIndexCount = 0;
		for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			totalVertexCount += scene->mMeshes[meshIdx]->mNumVertices;
			totalIndexCount += scene->mMeshes[meshIdx]->mNumFaces * 3;
		}
		asset->vertices.reserve(totalVertexCount);
		asset->indices.reserve(totalIndexCount);

		// MESHES
		for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++)
		{
			aiMesh* importedMesh = scene->mMeshes[meshIdx];

			AssetDiskMesh mesh{};
			mesh.firstVertex = static_cast<uint32_t>(asset->vertices.size());
			mesh.vertexCount = importedMesh->mNumVertices;
			mesh.firstIndex = static_cast<uint32_t>(asset->indices.size());
			mesh.materialIndex = importedMesh->mMaterialIndex;

			// VERTICES
			for (uint32_t i = 0; i < importedMesh->mNumVertices; i++)
			{
				const aiVector3D& importedPos = importedMesh->mVertices[i];
				const aiVector3D& importedNormal = importedMesh->HasNormals() ? importedMesh->mNormals[i] : aiVector3D(0, 0, 0);
				const aiVector3D& importedTangent = importedMesh->HasTangentsAndBitangents() ? importedMesh->mTangents[i] : aiVector3D(0, 0, 0);
				const aiVector3D& importedBitangent = importedMesh->HasTangentsAndBitangents() ? importedMesh->mBitangents[i] : aiVector3D(0, 0, 0);
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
			for (uint32_t j = 0; j < importedMesh->mNumFaces; j++)
			{
				const aiFace& importedFace = importedMesh->mFaces[j];

				asset->indices.push_back(importedFace.mIndices[0]);
				asset->indices.push_back(importedFace.mIndices[1]);
				asset->indices.push_back(importedFace.mIndices[2]);
			}

			mesh.indexCount = static_cast<uint32_t>(asset->indices.size()) - mesh.firstIndex;
			asset->meshes.push_back(mesh);
		}

		// MATERIALS
		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* pAIMaterial = scene->mMaterials[i];

			AssetDiskMaterial material{};
			aiString matName;
			if (pAIMaterial->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
			{
				material.name = matName.C_Str();
			}

			asset->materials.push_back(material);
		}

		// TEXTURES (loading standalone for now)
		const uint32_t numTextures = scene->mNumTextures;
		asset->textures.reserve(numTextures);
		for (uint32_t i = 0; i < numTextures; i++)
		{
			aiTexture* importedTexture = scene->mTextures[i];

			const PHX::Vec2u texSize = { static_cast<uint32_t>(importedTexture->mWidth), static_cast<uint32_t>(importedTexture->mHeight) };

			// Populate the texture data. Note from the assimp implementation:
			// The format of the data from the imported texture is always ARGB8888, meaning it's 32-bit aligned
			const uint64_t numBytes = static_cast<uint64_t>(texSize.GetX()) * texSize.GetY() * 4;
			void* ownedData = new char[numBytes];
			memcpy(ownedData, importedTexture->pcData, numBytes);
			AssetDiskTexture newTexture = AllocateTexture(importedTexture->mFilename.C_Str(), ownedData, texSize, 4, TEXTURE_TYPE::DIFFUSE);
			asset->textures.push_back(newTexture);
		}

		// MATERIALS (interpreted as textures)
		for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* currentAIMaterial = scene->mMaterials[i];
			AssetDiskMaterial* pMaterial = &asset->materials[i];

			// Get all the supported textures
			for (const auto& aiType : SUPPORTED_TEXTURE_TYPES)
			{
				uint32_t textureCount = currentAIMaterial->GetTextureCount(aiType);
				if (textureCount > 0)
				{
					// Warn if we have more than one diffuse texture, we don't currently support multiple texture of a given type
					if (textureCount > 1)
					{
						//LogWarning("More than one texture type (%u) detected for material %s! This is not currently supported", static_cast<uint32_t>(aiType), matName.C_Str());
						std::cout << "More than once texture type (" << static_cast<uint32_t>(aiType) << ") detected for material! This is not currently supported" << std::endl;
					}

					aiString texturePath;
					if (currentAIMaterial->GetTexture(aiType, 0, &texturePath) == AI_SUCCESS)
					{
						// We're only interested in the filenames, since we store the textures in a very specific directory
						std::filesystem::path textureFilePath = std::filesystem::path(texturePath.data);
						std::filesystem::path textureName = textureFilePath.filename();
						std::filesystem::path assetFilePath = std::filesystem::path(filePath).parent_path();
						std::filesystem::path textureSourceFilePath = (assetFilePath / textureFilePath);

						// Determine texture type
						auto texTypeIter = AI_TEXTURE_TO_INTERNAL.find(aiType);
						if (texTypeIter == AI_TEXTURE_TO_INTERNAL.end())
						{
							std::cout << "Failed to convert from aiTexture to the internal texture format! AiTexture type \"" << static_cast<uint32_t>(aiType) << "\"" << std::endl;
							continue;
						}
						TEXTURE_TYPE texType = texTypeIter->second;

						// Check if the texture is a DDS file
						std::string textureNameStr = textureName.string();
						std::string ext = textureName.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						bool isDDS = (ext == ".dds");

						AssetDiskTexture newTexture;
						if (isDDS)
						{
							newTexture = LoadDDS(textureSourceFilePath, texType);
						}
						else
						{
							newTexture = LoadTexture(textureSourceFilePath, texType);
						}

						if (newTexture.pData == nullptr)
						{
							std::cout << "Failed to load texture! \"" << textureSourceFilePath.string() << "\"" << std::endl;
							continue;
						}

						pMaterial->textureIndices.push_back(static_cast<uint32_t>(asset->textures.size()));
						asset->textures.push_back(newTexture);

						std::cout << "Material " << i << ": Loaded " << TEXTURE_TYPE_TO_STRING.at(texType).c_str() << " texture \"" << textureNameStr.c_str() << "\" from disk" << std::endl;
					}
				}
			}
		}

		return asset;
	}
}