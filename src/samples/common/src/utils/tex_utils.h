#pragma once

#include <filesystem>

#include "asset_importer.h"
#include "BSL/vec_types.h"

namespace Common
{
	// Creates an AssetDiskTexture that takes ownership of the provided pixel data buffer.
	// The data must have been allocated with new[]; it will be freed by FreeTextureData.
	AssetDiskTexture AllocateTexture(const char* pName, void* ownedData, BSL::Vec2u size, u32 bytesPerPixel, TEXTURE_TYPE type);

	// Loads an LDR texture (PNG, JPEG, etc.) from an absolute path using stb_image.
	// Returns an AssetDiskTexture with pData == nullptr on failure.
	AssetDiskTexture LoadTexture(const std::filesystem::path& filePath, TEXTURE_TYPE type);

	// Loads an LDR texture from a memory buffer (e.g. embedded FBX textures) using stb_image.
	// dataSize is the size of the compressed data buffer in bytes.
	// Returns an AssetDiskTexture with pData == nullptr on failure.
	AssetDiskTexture LoadTextureFromMemory(const void* pData, uint64_t dataSize, TEXTURE_TYPE type);

	// Loads an HDR texture (.hdr) from an absolute path using stb_image.
	// Returns an AssetDiskTexture with format R32G32B32A32_FLOAT and pData == nullptr on failure.
	AssetDiskTexture LoadHDRTexture(const std::filesystem::path& filePath, TEXTURE_TYPE type = TEXTURE_TYPE::MAX);

	// Parses a DDS file from an absolute path and returns an AssetDiskTexture with compressed format info and mip chain data.
	// Returns an AssetDiskTexture with pData == nullptr on failure.
	AssetDiskTexture LoadDDS(const std::filesystem::path& filePath, TEXTURE_TYPE type);

	// Frees all heap-allocated data within an AssetDiskTexture (pData, pName, mipLevels).
	// Safe to call on an already-freed or empty texture.
	void FreeTextureData(AssetDiskTexture& tex);
}
