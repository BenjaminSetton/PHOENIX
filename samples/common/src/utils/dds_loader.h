#pragma once

#include "asset_importer.h"

namespace Common
{
	// Parses a DDS file from disk and returns an AssetDiskTexture with compressed format info and mip chain data.
	// Returns an AssetDiskTexture with pData == nullptr on failure.
	AssetDiskTexture LoadDDS(const std::filesystem::path& filePath, TEXTURE_TYPE type);
}
