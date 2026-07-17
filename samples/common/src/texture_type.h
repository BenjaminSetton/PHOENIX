#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <PHX/types/vec_types.h>
#include <PHX/types/texture_desc.h>

#include "utils/asset_importer.h"

namespace Common
{
	struct TextureMipLevel
	{
		std::vector<uint8_t> data;
		PHX::Vec2u size      = {};
		PHX::u64 dataSize    = 0; // bytesPerPixel (uncompressed) or blockSize (compressed)
	};

	struct TextureType
	{
		std::string name;
		TEXTURE_TYPE type         = TEXTURE_TYPE::MAX;
		PHX::BASE_FORMAT format   = PHX::BASE_FORMAT::INVALID;
		std::vector<TextureMipLevel> mipLevels;

		bool IsCompressed() const { return format != PHX::BASE_FORMAT::INVALID; }
	};

	// TODO (future): Add a texture manifest to track all textures globally. Assets should hold
	// references (handles) to textures rather than embedding them. This enables texture sharing
	// across assets and reduces memory duplication.
}
