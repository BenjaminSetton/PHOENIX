#pragma once

#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <string>
#include <vector>

#include "../../common/src/utils/asset_importer.h"
#include "../../common/src/asset_manager.h"
#include "../../common/src/texture_type.h"
#include "../../common/src/serializer.h"
#include "../../common/src/asset_file_format.h"

struct AssetVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
	glm::uvec4 boneIndices;
	glm::vec4  boneWeights;
};

struct BoneInfo
{
	int32_t parentIndex = -1;
	uint32_t nodeIndex = 0;
	glm::mat4 offsetMatrix;
};

struct SkeletonData
{
	std::vector<BoneInfo> bones;
	std::vector<glm::mat4> nodeLocalTransforms;
	std::vector<int32_t> nodeParentIndices;
};

struct AnimationKey
{
	float time = 0.0f;
	glm::vec3 translation;
	glm::quat rotation;
	glm::vec3 scale;
};

struct AnimationChannel
{
	uint32_t nodeIndex = 0;
	std::vector<AnimationKey> keys;
};

struct AnimationClip
{
	std::string name;
	float duration = 0.0f;
	float ticksPerSecond = 0.0f;
	std::vector<AnimationChannel> channels;
};

struct AssetType
{
	std::vector<AssetVertex> vertices;
	std::vector<Common::AssetIndexType> indices;
	std::vector<Common::TextureType> textures;
	SkeletonData skeleton;
	std::vector<AnimationClip> animations;
};

using AssetManager = Common::AssetManager<AssetType>;

namespace Common
{
	template<>
	struct Serializer<AssetType>
	{
		static constexpr uint32_t TypeHash = HashTypeName("InstancedAnimation::AssetType");

		static void Write(std::ostream& os, const AssetType& asset, const std::filesystem::path& outputDir);
		static AssetType* Read(std::istream& is, const std::filesystem::path& inputDir);
		static AssetType* FromDisk(const AssetDisk& disk);
	};
}
