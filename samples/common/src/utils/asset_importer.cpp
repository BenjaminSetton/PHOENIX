
#include "asset_importer.h"

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION // Must only be defined once
#include <stb_image.h>

namespace Common
{
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

	AssetDiskTexture AllocateTexture(const void* const srcData, PHX::Vec2u size, PHX::u32 bytesPerPixel, TEXTURE_TYPE type)
	{
		AssetDiskTexture diskTex{};
		if (srcData == nullptr)
		{
			return diskTex;
		}

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

			const PHX::Vec2u texSize = { static_cast<uint32_t>(importedTexture->mWidth), static_cast<uint32_t>(importedTexture->mHeight) };

			// Populate the texture data. Note from the assimp implementation:
			// The format of the data from the imported texture is always ARGB8888, meaning it's 32-bit aligned
			AssetDiskTexture newTexture = AllocateTexture(importedTexture->pcData, texSize, 4, TEXTURE_TYPE::DIFFUSE);
			asset->textures.push_back(newTexture);
		}

		// MATERIALS (interpreted as textures)
		uint32_t numMaterials = scene->mNumMaterials;
		for (uint32_t i = 0; i < numMaterials; i++)
		{
			aiMaterial* currentAIMaterial = scene->mMaterials[i];

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
						std::filesystem::path textureSourceFilePath = (assetFilePath / textureName);

						// Load the image using stb_image
						std::string textureSourceFilePathStr = textureSourceFilePath.string();
						int width, height, channels;
						stbi_uc* pixels = stbi_load(textureSourceFilePathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
						if (pixels == nullptr)
						{
							//LogError("Failed to load texture! '%s'", textureSourceFilePathStr.c_str());
							std::cout << "Failed to load texture! \"" << textureSourceFilePathStr.c_str() << "\"" << std::endl;
							continue;
						}

						// Create a new texture and add it to the material
						auto texTypeIter = AI_TEXTURE_TO_INTERNAL.find(aiType);
						if (texTypeIter == AI_TEXTURE_TO_INTERNAL.end())
						{
							//LogError("Failed to convert from aiTexture to the internal texture format! AiTexture type '%s'", static_cast<uint32_t>(aiType));
							std::cout << "Failed to convert from aiTexture to the internal texture format! AiTexture type \"" << static_cast<uint32_t>(aiType) << "\"" << std::endl;
							continue;
						}
						TEXTURE_TYPE texType = texTypeIter->second;

						PHX::Vec2u texSize = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

						AssetDiskTexture newTexture = AllocateTexture(pixels, texSize, 4, texType);
						asset->textures.push_back(newTexture);

						/*LogInfo("\tMaterial %u: Loaded %s texture '%s' from disk",
							i,
							TextureTypeToString.at(AITextureToInternal.at(aiType)).c_str(),
							textureName.string().c_str()
						);*/

						std::cout << "Material " << i << ": Loaded " << TEXTURE_TYPE_TO_STRING.at(texType).c_str() << " texture \"" << textureName.string().c_str() << "\" from disk" << std::endl;
					}
				}
			}
		}

		return asset;
	}
}