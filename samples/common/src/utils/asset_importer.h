#pragma once

#include <filesystem>
#include <functional>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <memory.h>
#include <PHX/types/texture_desc.h>
#include <PHX/types/vec_types.h>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////
//
//	Imports an asset from disk. Returns an AssetDisk pointer, which should NEVER
//	be stored, and instead should be immediately converted into the samples'
//	own AssetType. In any serious use-case this would be an offline databuilding
//	step, but this works well enough for the sake of the samples
//
//////////////////////////////////////////////////////////////////////

namespace Common
{
	using AssetIndexType = uint32_t;

	enum class TEXTURE_TYPE
	{
		DIFFUSE = 0,
		SPECULAR,
		NORMAL,
		AMBIENT_OCCLUSION,
		METALLIC,
		ROUGHNESS,
		LIGHTMAP,
		
		MAX
	};

	struct AssetDiskVertex
	{
		PHX::Vec3f position;
		PHX::Vec3f normal;
		PHX::Vec3f tangent;
		PHX::Vec3f bitangent;
		PHX::Vec2f uv;
		PHX::Vec4u boneIndices;   // up to 4 bone influences (indices into bone array)
		PHX::Vec4f boneWeights;   // corresponding weights (sum to 1.0)
	};

	struct AssetDiskMipLevel
	{
		void* pData = nullptr;
		PHX::u32 width = 0;
		PHX::u32 height = 0;
		PHX::u64 dataSize = 0;
	};

	struct AssetDiskTexture
	{
		char* pName				= nullptr;
		void* pData				= nullptr;      // Base mip data (for uncompressed textures, or mip 0 of compressed)
		PHX::Vec2u size			= { 0, 0 };     // Base dimensions
		TEXTURE_TYPE type		= TEXTURE_TYPE::MAX;
		PHX::u32 bytesPerPixel	= 0;            // 0 for compressed textures

		// Compressed texture support
		PHX::BASE_FORMAT format = PHX::BASE_FORMAT::INVALID; 	// Set for DDS/compressed textures
		std::vector<AssetDiskMipLevel> mipLevels;    			// Per-mip data (mipLevels[0].pData == pData for compressed)
	};

	struct AssetDiskMaterial
	{
		std::string name;
		std::vector<PHX::u32> textureIndices;
	};

	struct AssetDiskMesh
	{
		PHX::u32 firstVertex = 0;
		PHX::u32 vertexCount = 0;
		PHX::u32 firstIndex = 0;
		PHX::u32 indexCount = 0;
		PHX::u32 materialIndex = 0;
	};

	struct AssetDiskBone
	{
		std::string name;
		glm::mat4 offsetMatrix;  // inverse bind pose
	};

	struct AssetDiskNode
	{
		std::string name;
		int32_t parentIndex = -1;  // -1 for root
		glm::mat4 localTransform;
	};

	struct AssetDiskAnimationKey
	{
		float time = 0.0f;
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
	};

	struct AssetDiskAnimationChannel
	{
		uint32_t nodeIndex = 0;  // index into AssetDisk::nodes
		std::vector<AssetDiskAnimationKey> keys;
	};

	struct AssetDiskAnimation
	{
		std::string name;
		float duration = 0.0f;       // in ticks
		float ticksPerSecond = 0.0f;
		std::vector<AssetDiskAnimationChannel> channels;
	};

	// A raw representation of an asset on disk. This is a generalization of common 3D asset extensions such as OBJ, FBX,
	// glTF, etc. Instances of AssetDisk are never stored. Instead, they must be converted into AssetResources instances
	// during import calls and only then is the data stored in the AssetManager
	struct AssetDisk
	{
		std::string name;
		std::vector<AssetDiskVertex> vertices;
		std::vector<AssetIndexType> indices;
		std::vector<AssetDiskTexture> textures;
		std::vector<AssetDiskMesh> meshes;
		std::vector<AssetDiskMaterial> materials;
		std::vector<AssetDiskBone> bones;
		std::vector<AssetDiskNode> nodes;
		std::vector<AssetDiskAnimation> animations;
	};

	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath, bool importAnimations = false);

	// Resolves a relative path against COMMON_ASSET_ROOT_DIR and SAMPLE_ASSET_ROOT_DIR.
	// Returns an empty path if the file cannot be found.
	std::filesystem::path FindAssetFile(const std::filesystem::path& relativePath);
}