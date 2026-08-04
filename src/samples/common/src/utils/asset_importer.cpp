
#include <algorithm>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <functional>
#include <iostream>
#include <unordered_map>

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
	static constexpr uint32_t MAX_BONE_INFLUENCES = 4;

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

	static glm::mat4 AiMatrixToGlm(const aiMatrix4x4& aiMat)
	{
		// Assimp stores matrices in row-major order (m[row][col]).
		// GLM's mat4 constructor takes arguments in column-major order.
		// Feed Assimp's columns directly as GLM constructor columns.
		return glm::mat4(
			aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
			aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
			aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
			aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		);
	}

	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath, bool importAnimations)
	{
		Assimp::Importer importer;

		// aiProcess_GlobalScale enables the ScaleProcess post-processing step.
		// The FBX importer auto-detects the file's unit system (e.g. centimeters)
		// and sets AI_CONFIG_APP_SCALE_KEY to convert to meters. With the default
		// global scale factor of 1.0, ScaleProcess applies only the importer's
		// file scale, so all models end up in meters regardless of source units
		uint32_t importFlags;
		if (importAnimations)
		{
			// For animated models, do NOT use aiProcess_PreTransformVertices as it bakes
			// the node hierarchy into vertices and destroys bone/skeleton data
			importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_FindInvalidData | aiProcess_GlobalScale;
		}
		else
		{
#if defined(FAST_IMPORT)
			importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices | aiProcess_GlobalScale;
#else
			importFlags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_FindInvalidData | aiProcess_PreTransformVertices | aiProcess_GlobalScale;
#endif
		}
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
		// Bone name → index map (shared across all meshes)
		std::unordered_map<std::string, uint32_t> boneNameToIndex;

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
				vertex.boneIndices = { 0, 0, 0, 0 };
				vertex.boneWeights = { 0.0f, 0.0f, 0.0f, 0.0f };

				asset->vertices.push_back(vertex);
			}

			// BONES (if animation import is enabled)
			if (importAnimations && importedMesh->HasBones())
			{
				uint32_t vertexOffset = mesh.firstVertex;

				for (uint32_t boneIdx = 0; boneIdx < importedMesh->mNumBones; boneIdx++)
				{
					aiBone* aiBoneData = importedMesh->mBones[boneIdx];
					std::string boneName = aiBoneData->mName.C_Str();

					// Register bone if not already seen
					uint32_t boneIndex;
					auto it = boneNameToIndex.find(boneName);
					if (it != boneNameToIndex.end())
					{
						boneIndex = it->second;
					}
					else
					{
						boneIndex = static_cast<uint32_t>(asset->bones.size());
						boneNameToIndex[boneName] = boneIndex;

						AssetDiskBone diskBone{};
						diskBone.name = boneName;
						diskBone.offsetMatrix = AiMatrixToGlm(aiBoneData->mOffsetMatrix);
						asset->bones.push_back(diskBone);
					}

					// Apply bone weights to vertices
					for (uint32_t weightIdx = 0; weightIdx < aiBoneData->mNumWeights; weightIdx++)
					{
						aiVertexWeight aiWeight = aiBoneData->mWeights[weightIdx];
						uint32_t vIdx = vertexOffset + aiWeight.mVertexId;

						if (vIdx >= asset->vertices.size())
							continue;

						AssetDiskVertex& v = asset->vertices[vIdx];

						// Find a free slot (up to MAX_BONE_INFLUENCES bone influences)
						bool assigned = false;
						for (uint32_t slot = 0; slot < MAX_BONE_INFLUENCES; slot++)
						{
							float currentWeight = (slot == 0) ? v.boneWeights.GetX() :
							                      (slot == 1) ? v.boneWeights.GetY() :
							                      (slot == 2) ? v.boneWeights.GetZ() :
							                                    v.boneWeights.GetW();

							if (currentWeight == 0.0f)
							{
								switch (slot)
								{
									case 0: v.boneIndices = { boneIndex, v.boneIndices.GetY(), v.boneIndices.GetZ(), v.boneIndices.GetW() };
									        v.boneWeights = { aiWeight.mWeight, v.boneWeights.GetY(), v.boneWeights.GetZ(), v.boneWeights.GetW() };
									        break;
									case 1: v.boneIndices = { v.boneIndices.GetX(), boneIndex, v.boneIndices.GetZ(), v.boneIndices.GetW() };
									        v.boneWeights = { v.boneWeights.GetX(), aiWeight.mWeight, v.boneWeights.GetZ(), v.boneWeights.GetW() };
									        break;
									case 2: v.boneIndices = { v.boneIndices.GetX(), v.boneIndices.GetY(), boneIndex, v.boneIndices.GetW() };
									        v.boneWeights = { v.boneWeights.GetX(), v.boneWeights.GetY(), aiWeight.mWeight, v.boneWeights.GetW() };
									        break;
									case 3: v.boneIndices = { v.boneIndices.GetX(), v.boneIndices.GetY(), v.boneIndices.GetZ(), boneIndex };
									        v.boneWeights = { v.boneWeights.GetX(), v.boneWeights.GetY(), v.boneWeights.GetZ(), aiWeight.mWeight };
									        break;
								}
							assigned = true;
							break;
							}
						}

						if (!assigned)
						{
							static bool warned = false;
							if (!warned)
							{
								std::cout << "[ASSET] Warning: Vertex " << vIdx << " has more than " << MAX_BONE_INFLUENCES << " bone influences. Extra weights will be dropped." << std::endl;
								warned = true;
							}
						}
					}
				}

				// Normalize bone weights for each vertex in this mesh
				for (uint32_t i = 0; i < importedMesh->mNumVertices; i++)
				{
					AssetDiskVertex& v = asset->vertices[vertexOffset + i];
					float totalWeight = v.boneWeights.GetX() + v.boneWeights.GetY() + v.boneWeights.GetZ() + v.boneWeights.GetW();
					if (totalWeight > 0.0f)
					{
						float invTotal = 1.0f / totalWeight;
						v.boneWeights = {
							v.boneWeights.GetX() * invTotal,
							v.boneWeights.GetY() * invTotal,
							v.boneWeights.GetZ() * invTotal,
							v.boneWeights.GetW() * invTotal
						};
					}
					else
					{
						// No bone weights — bind to bone 0 with full weight
						v.boneIndices = { 0, 0, 0, 0 };
						v.boneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
					}
				}
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

			if (importedTexture->mHeight == 0)
			{
				// Compressed embedded texture - data size is stored in mWidth.
				// Per Assimp docs, if mHeight is 0, mWidth holds the compressed data size
				uint64_t dataSize = static_cast<uint64_t>(importedTexture->mWidth);
				AssetDiskTexture newTexture = LoadTextureFromMemory(importedTexture->pcData, dataSize, TEXTURE_TYPE::DIFFUSE);
				if (newTexture.pData != nullptr)
				{
					asset->textures.push_back(newTexture);
				}
				else
				{
					std::cout << "[ASSET] Failed to decode embedded compressed texture: " << importedTexture->mFilename.C_Str() << std::endl;
				}
			}
			else
			{
				// Uncompressed embedded texture - data is in ARGB8888 format, guaranteed to be 32-bit aligned
				const PHX::Vec2u texSize = { static_cast<uint32_t>(importedTexture->mWidth), static_cast<uint32_t>(importedTexture->mHeight) };
				const uint64_t numBytes = static_cast<uint64_t>(texSize.GetX()) * texSize.GetY() * 4;
				void* ownedData = new char[numBytes];
				memcpy(ownedData, importedTexture->pcData, numBytes);
				AssetDiskTexture newTexture = AllocateTexture(importedTexture->mFilename.C_Str(), ownedData, texSize, 4, TEXTURE_TYPE::DIFFUSE);
				asset->textures.push_back(newTexture);
			}
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
						std::string texturePathStr = texturePath.C_Str();

						// Check if this is an embedded texture reference (e.g. "*0", "*1")
						if (texturePathStr.size() > 1 && texturePathStr[0] == '*')
						{
							uint32_t embeddedIndex = static_cast<uint32_t>(std::stoul(texturePathStr.substr(1)));
							if (embeddedIndex < asset->textures.size())
							{
								pMaterial->textureIndices.push_back(embeddedIndex);
								std::cout << "Material " << i << ": Linked embedded texture index " << embeddedIndex << std::endl;
							}
							else
							{
								std::cout << "[ASSET] Embedded texture index " << embeddedIndex << " out of range" << std::endl;
							}
							continue;
						}

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

		// NODE HIERARCHY (only for animated models)
		if (importAnimations && scene->mRootNode != nullptr)
		{
			std::unordered_map<std::string, uint32_t> nodeNameToIndex;

			// Recursive traversal to build flat node list
			std::function<void(const aiNode*, int32_t)> traverseNodes = [&](const aiNode* node, int32_t parentIndex)
			{
				uint32_t nodeIndex = static_cast<uint32_t>(asset->nodes.size());
				std::string nodeName = node->mName.C_Str();
				nodeNameToIndex[nodeName] = nodeIndex;

				AssetDiskNode diskNode{};
				diskNode.name = nodeName;
				diskNode.parentIndex = parentIndex;
				diskNode.localTransform = AiMatrixToGlm(node->mTransformation);
				asset->nodes.push_back(diskNode);

				for (uint32_t i = 0; i < node->mNumChildren; i++)
				{
					traverseNodes(node->mChildren[i], static_cast<int32_t>(nodeIndex));
				}
			};

			traverseNodes(scene->mRootNode, -1);

			// ANIMATIONS
			for (uint32_t animIdx = 0; animIdx < scene->mNumAnimations; animIdx++)
			{
				aiAnimation* aiAnim = scene->mAnimations[animIdx];

				AssetDiskAnimation diskAnim{};
				diskAnim.name = aiAnim->mName.C_Str();
				diskAnim.duration = static_cast<float>(aiAnim->mDuration);
				diskAnim.ticksPerSecond = (aiAnim->mTicksPerSecond != 0.0) ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.0f;

				for (uint32_t channelIdx = 0; channelIdx < aiAnim->mNumChannels; channelIdx++)
				{
					aiNodeAnim* aiChannel = aiAnim->mChannels[channelIdx];
					std::string channelNodeName = aiChannel->mNodeName.C_Str();

					auto nodeIt = nodeNameToIndex.find(channelNodeName);
					if (nodeIt == nodeNameToIndex.end())
					{
						std::cout << "[ASSET] Animation channel references unknown node: " << channelNodeName << std::endl;
						continue;
					}

					AssetDiskAnimationChannel diskChannel{};
					diskChannel.nodeIndex = nodeIt->second;

					// Assimp provides independent arrays for position, rotation, and scale
					// keys, each with potentially different counts and time values. Merging
					// them by index produces keyframes where the time comes from one key
					// type but the rotation/scale values are from a different time. Instead,
					// collect all unique time points and resample each component independently.

					auto sampleVec3Keys = [](const aiVectorKey* keys, uint32_t count, float t, const glm::vec3& defaultValue) -> glm::vec3 {
						if (count == 0) return defaultValue;
						if (count == 1) return glm::vec3(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
						if (t <= static_cast<float>(keys[0].mTime))
							return glm::vec3(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
						if (t >= static_cast<float>(keys[count - 1].mTime))
							return glm::vec3(keys[count - 1].mValue.x, keys[count - 1].mValue.y, keys[count - 1].mValue.z);
						for (uint32_t i = 0; i < count - 1; i++)
						{
							float t0 = static_cast<float>(keys[i].mTime);
							float t1 = static_cast<float>(keys[i + 1].mTime);
							if (t0 <= t && t1 >= t)
							{
								float range = t1 - t0;
								float factor = (range > 1e-6f) ? (t - t0) / range : 0.0f;
								return glm::mix(
									glm::vec3(keys[i].mValue.x, keys[i].mValue.y, keys[i].mValue.z),
									glm::vec3(keys[i + 1].mValue.x, keys[i + 1].mValue.y, keys[i + 1].mValue.z),
									factor);
							}
						}
						return glm::vec3(keys[count - 1].mValue.x, keys[count - 1].mValue.y, keys[count - 1].mValue.z);
					};

					auto sampleQuatKeys = [](const aiQuatKey* keys, uint32_t count, float t) -> glm::quat {
						if (count == 0) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
						if (count == 1) return glm::quat(keys[0].mValue.w, keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
						if (t <= static_cast<float>(keys[0].mTime))
							return glm::quat(keys[0].mValue.w, keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
						if (t >= static_cast<float>(keys[count - 1].mTime))
							return glm::quat(keys[count - 1].mValue.w, keys[count - 1].mValue.x, keys[count - 1].mValue.y, keys[count - 1].mValue.z);
						for (uint32_t i = 0; i < count - 1; i++)
						{
							float t0 = static_cast<float>(keys[i].mTime);
							float t1 = static_cast<float>(keys[i + 1].mTime);
							if (t0 <= t && t1 >= t)
							{
								float range = t1 - t0;
								float factor = (range > 1e-6f) ? (t - t0) / range : 0.0f;
								glm::quat q0(keys[i].mValue.w, keys[i].mValue.x, keys[i].mValue.y, keys[i].mValue.z);
								glm::quat q1(keys[i + 1].mValue.w, keys[i + 1].mValue.x, keys[i + 1].mValue.y, keys[i + 1].mValue.z);
								if (glm::dot(q0, q1) < 0.0f) q1 = -q1;
								return glm::slerp(q0, q1, factor);
							}
						}
						return glm::quat(keys[count - 1].mValue.w, keys[count - 1].mValue.x, keys[count - 1].mValue.y, keys[count - 1].mValue.z);
					};

					std::vector<float> keyTimes;
					for (uint32_t i = 0; i < aiChannel->mNumPositionKeys; i++)
						keyTimes.push_back(static_cast<float>(aiChannel->mPositionKeys[i].mTime));
					for (uint32_t i = 0; i < aiChannel->mNumRotationKeys; i++)
						keyTimes.push_back(static_cast<float>(aiChannel->mRotationKeys[i].mTime));
					for (uint32_t i = 0; i < aiChannel->mNumScalingKeys; i++)
						keyTimes.push_back(static_cast<float>(aiChannel->mScalingKeys[i].mTime));

					std::sort(keyTimes.begin(), keyTimes.end());
					keyTimes.erase(std::unique(keyTimes.begin(), keyTimes.end(),
						[](float a, float b) { return std::abs(a - b) < 1e-5f; }), keyTimes.end());

					diskChannel.keys.reserve(keyTimes.size());

					for (float t : keyTimes)
					{
						AssetDiskAnimationKey key{};
						key.time = t;
						key.translation = sampleVec3Keys(aiChannel->mPositionKeys, aiChannel->mNumPositionKeys, t, glm::vec3(0.0f));
						key.rotation = sampleQuatKeys(aiChannel->mRotationKeys, aiChannel->mNumRotationKeys, t);
						key.scale = sampleVec3Keys(aiChannel->mScalingKeys, aiChannel->mNumScalingKeys, t, glm::vec3(1.0f));
						diskChannel.keys.push_back(key);
					}

					diskAnim.channels.push_back(std::move(diskChannel));
				}

				std::cout << "[ASSET] Loaded animation: " << diskAnim.name << " (" << diskAnim.channels.size() << " channels, " << diskAnim.duration << " ticks)" << std::endl;

				// TEMP DEBUG: dump magnitudes to check for a unit-scale mismatch between
				// bind-pose node transforms and animation keyframe translations.
				if (!diskAnim.channels.empty() && !diskAnim.channels[0].keys.empty())
				{
					const AssetDiskAnimationChannel& dbgChannel = diskAnim.channels[0];
					const glm::vec3& dbgKeyTrans = dbgChannel.keys[0].translation;
					const glm::vec3& dbgKeyScale = dbgChannel.keys[0].scale;
					const glm::vec3 dbgNodeTrans = glm::vec3(asset->nodes[dbgChannel.nodeIndex].localTransform[3]);
					std::cout << "[ASSET][DEBUG] Channel0 node='" << asset->nodes[dbgChannel.nodeIndex].name
						<< "' keyTrans=(" << dbgKeyTrans.x << ", " << dbgKeyTrans.y << ", " << dbgKeyTrans.z << ")"
						<< " keyScale=(" << dbgKeyScale.x << ", " << dbgKeyScale.y << ", " << dbgKeyScale.z << ")"
						<< " nodeBindTrans=(" << dbgNodeTrans.x << ", " << dbgNodeTrans.y << ", " << dbgNodeTrans.z << ")"
						<< std::endl;

					// Dump scale for every channel in this animation to spot any outliers
					for (uint32_t dc = 0; dc < diskAnim.channels.size(); dc++)
					{
						const AssetDiskAnimationChannel& ch = diskAnim.channels[dc];
						if (ch.keys.empty()) continue;
						const glm::vec3& s = ch.keys[0].scale;
						if (s.x > 2.0f || s.y > 2.0f || s.z > 2.0f || s.x < 0.5f || s.y < 0.5f || s.z < 0.5f)
						{
							std::cout << "[ASSET][DEBUG] Outlier scale on node '" << asset->nodes[ch.nodeIndex].name
								<< "' scale=(" << s.x << ", " << s.y << ", " << s.z << ")" << std::endl;
						}
					}
				}

				asset->animations.push_back(std::move(diskAnim));
			}
		}

		return asset;
	}
}
