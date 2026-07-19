
#include "tex_utils.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Common
{
	// -------- DDS constants and helpers (moved from dds_loader.cpp) --------

	static constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

	static constexpr uint32_t DDSD_CAPS			= 0x1;
	static constexpr uint32_t DDSD_HEIGHT		= 0x2;
	static constexpr uint32_t DDSD_WIDTH		= 0x4;
	static constexpr uint32_t DDSD_PITCH		= 0x8;
	static constexpr uint32_t DDSD_PIXELFORMAT	= 0x1000;
	static constexpr uint32_t DDSD_MIPMAPCOUNT	= 0x20000;
	static constexpr uint32_t DDSD_LINEARSIZE	= 0x80000;
	static constexpr uint32_t DDSD_DEPTH		= 0x800000;

	static constexpr uint32_t DDPF_ALPHAPIXELS	= 0x1;
	static constexpr uint32_t DDPF_FOURCC		= 0x4;
	static constexpr uint32_t DDPF_RGB			= 0x40;

	static constexpr uint32_t DDSCAPS2_CUBEMAP	= 0x200;

#pragma pack(push, 1)
	struct DDSPixelFormat
	{
		uint32_t size;
		uint32_t flags;
		uint32_t fourCC;
		uint32_t rgbBitCount;
		uint32_t rBitMask;
		uint32_t gBitMask;
		uint32_t bBitMask;
		uint32_t aBitMask;
	};

	struct DDSHeader
	{
		uint32_t magic;
		uint32_t size;
		uint32_t flags;
		uint32_t height;
		uint32_t width;
		uint32_t pitchOrLinearSize;
		uint32_t depth;
		uint32_t mipMapCount;
		uint32_t reserved1[11];
		DDSPixelFormat pixelFormat;
		uint32_t caps;
		uint32_t caps2;
		uint32_t caps3;
		uint32_t caps4;
		uint32_t reserved2;
	};

	struct DDSHeaderDXT10
	{
		uint32_t dxgiFormat;
		uint32_t resourceDimension;
		uint32_t miscFlag;
		uint32_t arraySize;
		uint32_t miscFlags2;
	};
#pragma pack(pop)

	static constexpr uint32_t FOURCC_DXT1	= 0x31545844;
	static constexpr uint32_t FOURCC_DXT3	= 0x33545844;
	static constexpr uint32_t FOURCC_DXT5	= 0x35545844;
	static constexpr uint32_t FOURCC_BC4U	= 0x55344342;
	static constexpr uint32_t FOURCC_BC5U	= 0x55354342;
	static constexpr uint32_t FOURCC_ATI1	= 0x31495441;
	static constexpr uint32_t FOURCC_ATI2	= 0x32495441;
	static constexpr uint32_t FOURCC_DX10	= 0x30315844;

	static constexpr uint32_t DXGI_FORMAT_BC1_UNORM			= 71;
	static constexpr uint32_t DXGI_FORMAT_BC1_UNORM_SRGB	= 72;
	static constexpr uint32_t DXGI_FORMAT_BC3_UNORM			= 77;
	static constexpr uint32_t DXGI_FORMAT_BC3_UNORM_SRGB	= 78;
	static constexpr uint32_t DXGI_FORMAT_BC4_UNORM			= 80;
	static constexpr uint32_t DXGI_FORMAT_BC4_SNORM			= 81;
	static constexpr uint32_t DXGI_FORMAT_BC5_UNORM			= 83;
	static constexpr uint32_t DXGI_FORMAT_BC5_SNORM			= 84;
	static constexpr uint32_t DXGI_FORMAT_BC6H_UF16			= 95;
	static constexpr uint32_t DXGI_FORMAT_BC6H_SF16			= 96;
	static constexpr uint32_t DXGI_FORMAT_BC7_TYPELESS		= 97;
	static constexpr uint32_t DXGI_FORMAT_BC7_UNORM			= 98;
	static constexpr uint32_t DXGI_FORMAT_BC7_UNORM_SRGB	= 99;

	static PHX::BASE_FORMAT FourCCToBaseFormat(uint32_t fourCC, const DDSHeaderDXT10* pDXT10)
	{
		if (pDXT10 != nullptr)
		{
			switch (pDXT10->dxgiFormat)
			{
			case DXGI_FORMAT_BC1_UNORM:			return PHX::BASE_FORMAT::BC1_RGBA_UNORM;
			case DXGI_FORMAT_BC1_UNORM_SRGB:	return PHX::BASE_FORMAT::BC1_RGBA_SRGB;
			case DXGI_FORMAT_BC3_UNORM:			return PHX::BASE_FORMAT::BC3_UNORM;
			case DXGI_FORMAT_BC3_UNORM_SRGB:	return PHX::BASE_FORMAT::BC3_SRGB;
			case DXGI_FORMAT_BC4_UNORM:			return PHX::BASE_FORMAT::BC4_UNORM;
			case DXGI_FORMAT_BC4_SNORM:			return PHX::BASE_FORMAT::BC4_SNORM;
			case DXGI_FORMAT_BC5_UNORM:			return PHX::BASE_FORMAT::BC5_UNORM;
			case DXGI_FORMAT_BC5_SNORM:			return PHX::BASE_FORMAT::BC5_SNORM;
			case DXGI_FORMAT_BC6H_UF16:			return PHX::BASE_FORMAT::BC6H_UFLOAT;
			case DXGI_FORMAT_BC6H_SF16:			return PHX::BASE_FORMAT::BC6H_SFLOAT;
			case DXGI_FORMAT_BC7_UNORM:			return PHX::BASE_FORMAT::BC7_UNORM;
			case DXGI_FORMAT_BC7_UNORM_SRGB:	return PHX::BASE_FORMAT::BC7_SRGB;
			default:							return PHX::BASE_FORMAT::INVALID;
			}
		}

		switch (fourCC)
		{
		case FOURCC_DXT1:	return PHX::BASE_FORMAT::BC1_RGBA_UNORM;
		case FOURCC_DXT3:	return PHX::BASE_FORMAT::BC3_UNORM;
		case FOURCC_DXT5:	return PHX::BASE_FORMAT::BC3_UNORM;
		case FOURCC_BC4U:
		case FOURCC_ATI1:	return PHX::BASE_FORMAT::BC4_UNORM;
		case FOURCC_BC5U:
		case FOURCC_ATI2:	return PHX::BASE_FORMAT::BC5_UNORM;
		default:			return PHX::BASE_FORMAT::INVALID;
		}
	}

	static uint32_t GetBlockSize(PHX::BASE_FORMAT format)
	{
		switch (format)
		{
		case PHX::BASE_FORMAT::BC1_RGB_UNORM:
		case PHX::BASE_FORMAT::BC1_RGB_SRGB:
		case PHX::BASE_FORMAT::BC1_RGBA_UNORM:
		case PHX::BASE_FORMAT::BC1_RGBA_SRGB:
		case PHX::BASE_FORMAT::BC4_UNORM:
		case PHX::BASE_FORMAT::BC4_SNORM:
			return 8;
		case PHX::BASE_FORMAT::BC3_UNORM:
		case PHX::BASE_FORMAT::BC3_SRGB:
		case PHX::BASE_FORMAT::BC5_UNORM:
		case PHX::BASE_FORMAT::BC5_SNORM:
		case PHX::BASE_FORMAT::BC6H_UFLOAT:
		case PHX::BASE_FORMAT::BC6H_SFLOAT:
		case PHX::BASE_FORMAT::BC7_UNORM:
		case PHX::BASE_FORMAT::BC7_SRGB:
			return 16;
		default:
			return 0;
		}
	}

	static uint64_t CalculateCompressedMipSize(uint32_t width, uint32_t height, uint32_t blockSize)
	{
		uint32_t blocksX = (width + 3) / 4;
		uint32_t blocksY = (height + 3) / 4;
		return static_cast<uint64_t>(blocksX) * blocksY * blockSize;
	}

	AssetDiskTexture AllocateTexture(const char* pName, void* ownedData, PHX::Vec2u size, PHX::u32 bytesPerPixel, TEXTURE_TYPE type)
	{
		AssetDiskTexture diskTex{};
		if (ownedData == nullptr)
		{
			return diskTex;
		}

		const uint32_t nameSize = static_cast<uint32_t>(strlen(pName)) + 1;
		diskTex.pName = new char[nameSize];
		strcpy_s(diskTex.pName, nameSize, pName);

		diskTex.pData = ownedData;
		diskTex.size = size;
		diskTex.type = type;
		diskTex.bytesPerPixel = bytesPerPixel;

		return diskTex;
	}

	AssetDiskTexture LoadTexture(const std::filesystem::path& filePath, TEXTURE_TYPE type)
	{
		AssetDiskTexture result{};

		std::string filePathStr = filePath.string();
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_uc* pixels = stbi_load(filePathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (pixels == nullptr)
		{
			std::cout << "[TEXTURE] Failed to load texture '" << filePathStr << "': " << stbi_failure_reason() << std::endl;
			return result;
		}

		const uint32_t w = static_cast<uint32_t>(width);
		const uint32_t h = static_cast<uint32_t>(height);
		const uint64_t numBytes = static_cast<uint64_t>(w) * h * 4; // RGBA8

		void* ownedData = new char[numBytes];
		memcpy(ownedData, pixels, numBytes);
		stbi_image_free(pixels);

		result = AllocateTexture(filePath.filename().string().c_str(), ownedData, { w, h }, 4, type);

		std::cout << "[TEXTURE] Loaded '" << filePath.filename().string() << "' (" << width << "x" << height << ", " << channels << " channels, forced RGBA8)" << std::endl;

		return result;
	}

	AssetDiskTexture LoadHDRTexture(const std::filesystem::path& filePath, TEXTURE_TYPE type)
	{
		AssetDiskTexture result{};

		std::string filePathStr = filePath.string();
		int width = 0;
		int height = 0;
		int channels = 0;
		float* pixels = stbi_loadf(filePathStr.c_str(), &width, &height, &channels, 4);
		if (pixels == nullptr)
		{
			std::cout << "[TEXTURE] Failed to load HDR texture '" << filePathStr << "': " << stbi_failure_reason() << std::endl;
			return result;
		}

		const uint32_t w = static_cast<uint32_t>(width);
		const uint32_t h = static_cast<uint32_t>(height);
		const uint64_t numBytes = static_cast<uint64_t>(w) * h * 4 * sizeof(float);

		void* ownedData = new char[numBytes];
		memcpy(ownedData, pixels, numBytes);
		stbi_image_free(pixels);

		result = AllocateTexture(filePath.filename().string().c_str(), ownedData, { w, h }, 16, type);
		result.format = PHX::BASE_FORMAT::R32G32B32A32_FLOAT;

		std::cout << "[TEXTURE] Loaded HDR '" << filePath.filename().string() << "' (" << width << "x" << height << ", " << channels << " channels, forced RGBA32F)" << std::endl;

		return result;
	}

	AssetDiskTexture LoadDDS(const std::filesystem::path& filePath, TEXTURE_TYPE type)
	{
		AssetDiskTexture result{};
		result.type = type;

		std::ifstream file(filePath, std::ios::binary);
		if (!file.is_open())
		{
			std::cout << "[DDS] Failed to open file: " << filePath.string() << std::endl;
			return result;
		}

		DDSHeader header{};
		file.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));

		if (header.magic != DDS_MAGIC)
		{
			std::cout << "[DDS] Invalid magic number in file: " << filePath.string() << std::endl;
			return result;
		}

		if (header.size != 124)
		{
			std::cout << "[DDS] Unexpected header size: " << header.size << std::endl;
			return result;
		}

		DDSHeaderDXT10 dxt10{};
		DDSHeaderDXT10* pDXT10 = nullptr;
		if (header.pixelFormat.fourCC == FOURCC_DX10)
		{
			file.read(reinterpret_cast<char*>(&dxt10), sizeof(DDSHeaderDXT10));
			pDXT10 = &dxt10;
		}

		PHX::BASE_FORMAT format = FourCCToBaseFormat(header.pixelFormat.fourCC, pDXT10);
		if (format == PHX::BASE_FORMAT::INVALID)
		{
			std::cout << "[DDS] Unsupported FourCC/DXGI format in file: " << filePath.string() << std::endl;
			return result;
		}

		uint32_t blockSize = GetBlockSize(format);
		if (blockSize == 0)
		{
			std::cout << "[DDS] Unknown block size for format" << std::endl;
			return result;
		}

		uint32_t width = header.width;
		uint32_t height = header.height;
		uint32_t mipCount = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;

		const std::string filenameStr = filePath.filename().string();
		const uint32_t fileNameSize = static_cast<uint32_t>(filenameStr.size()) + 1;
		result.pName = new char[fileNameSize];
		strcpy_s(result.pName, fileNameSize, filenameStr.c_str());
		result.size = { width, height };
		result.format = format;
		result.bytesPerPixel = 0;
		result.mipLevels.resize(mipCount);

		uint32_t curWidth = width;
		uint32_t curHeight = height;

		for (uint32_t mip = 0; mip < mipCount; mip++)
		{
			uint64_t mipSize = CalculateCompressedMipSize(curWidth, curHeight, blockSize);

			void* pMipData = new char[mipSize];
			file.read(reinterpret_cast<char*>(pMipData), mipSize);

			result.mipLevels[mip].pData = pMipData;
			result.mipLevels[mip].width = curWidth;
			result.mipLevels[mip].height = curHeight;
			result.mipLevels[mip].dataSize = mipSize;

			if (mip == 0)
			{
				result.pData = pMipData;
			}

			curWidth = (curWidth > 1) ? curWidth / 2 : 1;
			curHeight = (curHeight > 1) ? curHeight / 2 : 1;
		}

		std::cout << "[DDS] Loaded \"" << filePath.filename().string() << "\" (" << width << "x" << height << ", " << mipCount << " mips)" << std::endl;

		return result;
	}

	void FreeTextureData(AssetDiskTexture& tex)
	{
		if (tex.pName != nullptr)
		{
			delete[] tex.pName;
			tex.pName = nullptr;
		}

		// For DDS textures, mip 0's pData points into mipLevels[0].pData, so we only
		// need to free the mip chain. For non-DDS textures, pData is a standalone allocation.
		if (!tex.mipLevels.empty())
		{
			for (auto& mip : tex.mipLevels)
			{
				if (mip.pData != nullptr)
				{
					delete[] static_cast<char*>(mip.pData);
					mip.pData = nullptr;
				}
			}
			tex.mipLevels.clear();
			tex.pData = nullptr; // pData was aliased to mipLevels[0]
		}
		else if (tex.pData != nullptr)
		{
			delete[] static_cast<char*>(tex.pData);
			tex.pData = nullptr;
		}
	}
}
