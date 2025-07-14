#pragma once

#include <filesystem>
#include <functional>
#include <PHX/types/vec_types.h>
#include <string>
#include <vector>
#include <memory.h>

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

	struct AssetDiskTexture
	{
		void* pData				= nullptr;
		PHX::Vec2u size			= { 0, 0 };
		TEXTURE_TYPE type		= TEXTURE_TYPE::MAX;
		PHX::u32 bytesPerPixel	= 0;
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
	};

	std::shared_ptr<AssetDisk> ImportAsset(std::filesystem::path filePath);
}