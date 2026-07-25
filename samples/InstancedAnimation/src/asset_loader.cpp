#include "asset_loader.h"

#include "../../common/src/serializer_texture.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

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

		// Write skeleton
		uint32_t boneCount = static_cast<uint32_t>(asset.skeleton.bones.size());
		WriteTrivial(os, boneCount);
		for (uint32_t i = 0; i < boneCount; i++)
		{
			const BoneInfo& bone = asset.skeleton.bones[i];
			WriteTrivial(os, bone.parentIndex);
			WriteTrivial(os, bone.nodeIndex);
			WriteTrivial(os, bone.offsetMatrix);
		}

		uint32_t nodeTransformCount = static_cast<uint32_t>(asset.skeleton.nodeLocalTransforms.size());
		WriteTrivial(os, nodeTransformCount);
		for (uint32_t i = 0; i < nodeTransformCount; i++)
		{
			WriteTrivial(os, asset.skeleton.nodeLocalTransforms[i]);
		}

		// Write node parent indices
		WriteTrivial(os, nodeTransformCount); // same count
		for (uint32_t i = 0; i < nodeTransformCount; i++)
		{
			WriteTrivial(os, asset.skeleton.nodeParentIndices[i]);
		}

		// Write animations
		uint32_t animCount = static_cast<uint32_t>(asset.animations.size());
		WriteTrivial(os, animCount);
		for (uint32_t i = 0; i < animCount; i++)
		{
			const AnimationClip& clip = asset.animations[i];
			WriteString(os, clip.name);
			WriteTrivial(os, clip.duration);
			WriteTrivial(os, clip.ticksPerSecond);

			uint32_t channelCount = static_cast<uint32_t>(clip.channels.size());
			WriteTrivial(os, channelCount);
			for (uint32_t j = 0; j < channelCount; j++)
			{
				const AnimationChannel& channel = clip.channels[j];
				WriteTrivial(os, channel.nodeIndex);

				uint32_t keyCount = static_cast<uint32_t>(channel.keys.size());
				WriteTrivial(os, keyCount);
				for (uint32_t k = 0; k < keyCount; k++)
				{
					const AnimationKey& key = channel.keys[k];
					WriteTrivial(os, key.time);
					WriteTrivial(os, key.translation);
					WriteTrivial(os, key.rotation);
					WriteTrivial(os, key.scale);
				}
			}
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

		// Read textures
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

		// Read skeleton
		uint32_t boneCount = ReadTrivial<uint32_t>(is);
		asset->skeleton.bones.resize(boneCount);
		for (uint32_t i = 0; i < boneCount; i++)
		{
			asset->skeleton.bones[i].parentIndex = ReadTrivial<int32_t>(is);
			asset->skeleton.bones[i].nodeIndex = ReadTrivial<uint32_t>(is);
			asset->skeleton.bones[i].offsetMatrix = ReadTrivial<glm::mat4>(is);
		}

		uint32_t nodeTransformCount = ReadTrivial<uint32_t>(is);
		asset->skeleton.nodeLocalTransforms.resize(nodeTransformCount);
		for (uint32_t i = 0; i < nodeTransformCount; i++)
		{
			asset->skeleton.nodeLocalTransforms[i] = ReadTrivial<glm::mat4>(is);
		}

		// Read node parent indices
		uint32_t nodeParentCount = ReadTrivial<uint32_t>(is);
		asset->skeleton.nodeParentIndices.resize(nodeParentCount);
		for (uint32_t i = 0; i < nodeParentCount; i++)
		{
			asset->skeleton.nodeParentIndices[i] = ReadTrivial<int32_t>(is);
		}

		// Read animations
		uint32_t animCount = ReadTrivial<uint32_t>(is);
		asset->animations.reserve(animCount);
		for (uint32_t i = 0; i < animCount; i++)
		{
			AnimationClip clip;
			clip.name = ReadString(is);
			clip.duration = ReadTrivial<float>(is);
			clip.ticksPerSecond = ReadTrivial<float>(is);

			uint32_t channelCount = ReadTrivial<uint32_t>(is);
			clip.channels.resize(channelCount);
			for (uint32_t j = 0; j < channelCount; j++)
			{
				clip.channels[j].nodeIndex = ReadTrivial<uint32_t>(is);

				uint32_t keyCount = ReadTrivial<uint32_t>(is);
				clip.channels[j].keys.resize(keyCount);
				for (uint32_t k = 0; k < keyCount; k++)
				{
					AnimationKey& key = clip.channels[j].keys[k];
					key.time = ReadTrivial<float>(is);
					key.translation = ReadTrivial<glm::vec3>(is);
					key.rotation = ReadTrivial<glm::quat>(is);
					key.scale = ReadTrivial<glm::vec3>(is);
				}
			}

			asset->animations.push_back(std::move(clip));
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
			newVert.position = glm::vec3(diskVert.position.GetX(), diskVert.position.GetY(), diskVert.position.GetZ());
			newVert.normal = glm::vec3(diskVert.normal.GetX(), diskVert.normal.GetY(), diskVert.normal.GetZ());
			newVert.uv = glm::vec2(diskVert.uv.GetX(), diskVert.uv.GetY());
			newVert.boneIndices = glm::uvec4(diskVert.boneIndices.GetX(), diskVert.boneIndices.GetY(), diskVert.boneIndices.GetZ(), diskVert.boneIndices.GetW());
			newVert.boneWeights = glm::vec4(diskVert.boneWeights.GetX(), diskVert.boneWeights.GetY(), diskVert.boneWeights.GetZ(), diskVert.boneWeights.GetW());

			asset->vertices.push_back(newVert);
		}

		// INDICES
		asset->indices = disk.indices;

		// TEXTURES
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

		// SKELETON — build bone info from AssetDiskBone
		// We need to map bone names to node indices to establish parent relationships
		asset->skeleton.bones.resize(disk.bones.size());
		for (uint32_t i = 0; i < disk.bones.size(); i++)
		{
			asset->skeleton.bones[i].parentIndex = -1; // Will be resolved below
			asset->skeleton.bones[i].offsetMatrix = disk.bones[i].offsetMatrix;
		}

		// Build node local transforms from AssetDiskNode
		asset->skeleton.nodeLocalTransforms.resize(disk.nodes.size());
		asset->skeleton.nodeParentIndices.resize(disk.nodes.size());
		for (uint32_t i = 0; i < disk.nodes.size(); i++)
		{
			asset->skeleton.nodeLocalTransforms[i] = disk.nodes[i].localTransform;
			asset->skeleton.nodeParentIndices[i] = disk.nodes[i].parentIndex;
		}

		// Map bone names to node indices for parent resolution
		// Each bone corresponds to a node in the hierarchy. Find the node by name,
		// then use that node's parent index to set the bone's parent.
		std::unordered_map<std::string, uint32_t> nodeNameToIndex;
		for (uint32_t i = 0; i < disk.nodes.size(); i++)
		{
			nodeNameToIndex[disk.nodes[i].name] = i;
		}

		for (uint32_t i = 0; i < disk.bones.size(); i++)
		{
			auto it = nodeNameToIndex.find(disk.bones[i].name);
			if (it != nodeNameToIndex.end())
			{
				uint32_t nodeIdx = it->second;
				asset->skeleton.bones[i].nodeIndex = nodeIdx;
				int32_t nodeParent = disk.nodes[nodeIdx].parentIndex;

				// Find which bone index corresponds to the parent node
				if (nodeParent >= 0)
				{
					const std::string& parentName = disk.nodes[nodeParent].name;
					auto parentIt = nodeNameToIndex.find(parentName);
					if (parentIt != nodeNameToIndex.end())
					{
						// Check if the parent node is also a bone
						for (uint32_t j = 0; j < disk.bones.size(); j++)
						{
							if (disk.bones[j].name == parentName)
							{
								asset->skeleton.bones[i].parentIndex = static_cast<int32_t>(j);
								break;
							}
						}
					}
				}
			}
		}

		// ANIMATIONS
		asset->animations.reserve(disk.animations.size());
		for (uint32_t i = 0; i < disk.animations.size(); i++)
		{
			const AssetDiskAnimation& diskAnim = disk.animations[i];
			AnimationClip clip;
			clip.name = diskAnim.name;
			clip.duration = diskAnim.duration;
			clip.ticksPerSecond = diskAnim.ticksPerSecond;

			clip.channels.reserve(diskAnim.channels.size());
			for (uint32_t j = 0; j < diskAnim.channels.size(); j++)
			{
				const AssetDiskAnimationChannel& diskChannel = diskAnim.channels[j];
				AnimationChannel channel;
				channel.nodeIndex = diskChannel.nodeIndex;

				channel.keys.reserve(diskChannel.keys.size());
				for (uint32_t k = 0; k < diskChannel.keys.size(); k++)
				{
					const AssetDiskAnimationKey& diskKey = diskChannel.keys[k];
					AnimationKey key;
					key.time = diskKey.time;
					key.translation = diskKey.translation;
					key.rotation = diskKey.rotation;
					key.scale = diskKey.scale;
					channel.keys.push_back(key);
				}

				clip.channels.push_back(std::move(channel));
			}

			asset->animations.push_back(std::move(clip));
		}

		return asset;
	}
}
