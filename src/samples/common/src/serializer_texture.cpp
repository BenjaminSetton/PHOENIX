#include "serializer_texture.h"

#include <cstring>
#include <iostream>

namespace Common
{
	static constexpr uint32_t TEXTURE_TYPE_HASH = HashTypeName("Common::TextureType");

	void Serializer<TextureType>::Write(std::ostream& os, const TextureType& texture, const std::filesystem::path& outputDir)
	{
		(void)outputDir; // Textures are self-contained — no external files needed
		// Write header
		BSL::WriteHeader(os, TEXTURE_MAGIC, TEXTURE_FORMAT_VERSION, TEXTURE_TYPE_HASH);

		// Write texture metadata
		BSL::WriteString(os, texture.name);
		BSL::WriteTrivial(os, static_cast<uint32_t>(texture.type));
		BSL::WriteTrivial(os, static_cast<uint32_t>(texture.format));

		// Write mip levels
		uint32_t mipCount = static_cast<uint32_t>(texture.mipLevels.size());
		BSL::WriteTrivial(os, mipCount);

		for (uint32_t i = 0; i < mipCount; i++)
		{
			const TextureMipLevel& mip = texture.mipLevels[i];

			BSL::WriteTrivial(os, mip.size.GetX());
			BSL::WriteTrivial(os, mip.size.GetY());
			BSL::WriteTrivial(os, mip.dataSize);

			uint64_t dataSize = static_cast<uint64_t>(mip.data.size());
			BSL::WriteTrivial(os, dataSize);
			os.write(reinterpret_cast<const char*>(mip.data.data()), dataSize);
		}
	}

	TextureType* Serializer<TextureType>::Read(std::istream& is, const std::filesystem::path& inputDir)
	{
		(void)inputDir; // Textures are self-contained — no external files needed

		TextureType* texture = new TextureType{};

		// Read and validate header
		if (!BSL::ReadAndValidateHeader(is, TEXTURE_MAGIC, TEXTURE_FORMAT_VERSION, TEXTURE_TYPE_HASH))
		{
			std::cout << "[ASSET] Failed to validate texture file header" << std::endl;
			delete texture;
			return nullptr;
		}

		// Read texture metadata
		texture->name   = BSL::ReadString(is);
		texture->type   = static_cast<TEXTURE_TYPE>(BSL::ReadTrivial<uint32_t>(is));
		texture->format = static_cast<PHX::BASE_FORMAT>(BSL::ReadTrivial<uint32_t>(is));

		// Read mip levels
		uint32_t mipCount = BSL::ReadTrivial<uint32_t>(is);
		texture->mipLevels.resize(mipCount);

		for (uint32_t i = 0; i < mipCount; i++)
		{
			TextureMipLevel& mip = texture->mipLevels[i];

			uint32_t width  = BSL::ReadTrivial<uint32_t>(is);
			uint32_t height = BSL::ReadTrivial<uint32_t>(is);
			mip.size = { width, height };
			mip.dataSize = BSL::ReadTrivial<u64>(is);

			uint64_t dataSize = BSL::ReadTrivial<uint64_t>(is);
			mip.data.resize(dataSize);
			is.read(reinterpret_cast<char*>(mip.data.data()), dataSize);
		}

		return texture;
	}

	TextureType* Serializer<TextureType>::FromDisk(const AssetDiskTexture& diskTex)
	{
		TextureType* texture = new TextureType{};

		texture->name   = diskTex.pName ? diskTex.pName : "UnnamedTexture";
		texture->type   = diskTex.type;
		texture->format = diskTex.format;

		if (!diskTex.mipLevels.empty())
		{
			// Compressed texture — copy mip chain
			texture->mipLevels.resize(diskTex.mipLevels.size());
			for (size_t mip = 0; mip < diskTex.mipLevels.size(); mip++)
			{
				uint64_t dataSize = diskTex.mipLevels[mip].dataSize;
				texture->mipLevels[mip].size = { diskTex.mipLevels[mip].width, diskTex.mipLevels[mip].height };
				texture->mipLevels[mip].dataSize = dataSize;

				const uint8_t* pSrc = static_cast<const uint8_t*>(diskTex.mipLevels[mip].pData);
				texture->mipLevels[mip].data.assign(pSrc, pSrc + dataSize);
			}
		}
		else
		{
			// Uncompressed texture — single mip level from flat data
			TextureMipLevel mip{};
			mip.size = diskTex.size;
			mip.dataSize = static_cast<u64>(diskTex.size.GetX()) * diskTex.size.GetY() * diskTex.bytesPerPixel;

			const uint8_t* pSrc = static_cast<const uint8_t*>(diskTex.pData);
			mip.data.assign(pSrc, pSrc + mip.dataSize);
			texture->mipLevels.push_back(std::move(mip));
		}

		return texture;
	}
}
