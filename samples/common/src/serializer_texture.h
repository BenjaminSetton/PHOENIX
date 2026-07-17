#pragma once

#include <fstream>

#include "serializer.h"
#include "texture_type.h"
#include "asset_file_format.h"

namespace Common
{
	template<>
	struct Serializer<TextureType>
	{
		static constexpr uint32_t TypeHash = HashTypeName("Common::TextureType");

		// Serialize a TextureType to a binary stream (including header)
		static void Write(std::ostream& os, const TextureType& texture, const std::filesystem::path& outputDir);

		// Deserialize a TextureType from a binary stream (including header validation)
		static TextureType* Read(std::istream& is, const std::filesystem::path& inputDir);

		// Convert an AssetDiskTexture to a TextureType
		static TextureType* FromDisk(const AssetDiskTexture& diskTex);
	};
}
