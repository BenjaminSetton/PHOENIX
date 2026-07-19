#pragma once

#include <filesystem>
#include <functional>
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
	};

	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath);

	// Resolves a relative path against COMMON_ASSET_ROOT_DIR and SAMPLE_ASSET_ROOT_DIR.
	// Returns an empty path if the file cannot be found.
	std::filesystem::path FindAssetFile(const std::filesystem::path& relativePath);
}