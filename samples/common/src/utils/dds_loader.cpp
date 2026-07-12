
#include <fstream>
#include <iostream>
#include <cstring>

#include "dds_loader.h"

namespace Common
{
	// DDS magic number
	static constexpr uint32_t DDS_MAGIC = 0x20534444; // "DDS "

	// DDS header flags
	static constexpr uint32_t DDSD_CAPS			= 0x1;
	static constexpr uint32_t DDSD_HEIGHT		= 0x2;
	static constexpr uint32_t DDSD_WIDTH		= 0x4;
	static constexpr uint32_t DDSD_PITCH		= 0x8;
	static constexpr uint32_t DDSD_PIXELFORMAT	= 0x1000;
	static constexpr uint32_t DDSD_MIPMAPCOUNT	= 0x20000;
	static constexpr uint32_t DDSD_LINEARSIZE	= 0x80000;
	static constexpr uint32_t DDSD_DEPTH		= 0x800000;

	// DDS pixel format flags
	static constexpr uint32_t DDPF_ALPHAPIXELS	= 0x1;
	static constexpr uint32_t DDPF_FOURCC		= 0x4;
	static constexpr uint32_t DDPF_RGB			= 0x40;

	// DDS caps2 flags
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

	// DDS Header DX10 extension (for BC6H/BC7 which require DX10 header)
	struct DDSHeaderDXT10
	{
		uint32_t dxgiFormat;
		uint32_t resourceDimension;
		uint32_t miscFlag;
		uint32_t arraySize;
		uint32_t miscFlags2;
	};
#pragma pack(pop)

	// FourCC values for BC formats
	static constexpr uint32_t FOURCC_DXT1	= 0x31545844; // "DXT1"
	static constexpr uint32_t FOURCC_DXT3	= 0x33545844; // "DXT3"
	static constexpr uint32_t FOURCC_DXT5	= 0x35545844; // "DXT5"
	static constexpr uint32_t FOURCC_BC4U	= 0x55344342; // "BC4U"
	static constexpr uint32_t FOURCC_BC5U	= 0x55354342; // "BC5U"
	static constexpr uint32_t FOURCC_ATI1	= 0x31495441; // "ATI1"
	static constexpr uint32_t FOURCC_ATI2	= 0x32495441; // "ATI2"
	static constexpr uint32_t FOURCC_DX10	= 0x30315844; // "DX10"

	// DXGI formats for BC1-BC7
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
		case FOURCC_DXT1:	return PHX::BASE_FORMAT::BC1_RGBA_UNORM; // DXT1 can have alpha, use RGBA variant
		case FOURCC_DXT3:	return PHX::BASE_FORMAT::BC3_UNORM;      // DXT3 maps to BC3
		case FOURCC_DXT5:	return PHX::BASE_FORMAT::BC3_UNORM;      // DXT5 maps to BC3
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
		// BC formats use 4x4 blocks. Each block is 'blockSize' bytes.
		uint32_t blocksX = (width + 3) / 4;
		uint32_t blocksY = (height + 3) / 4;
		return static_cast<uint64_t>(blocksX) * blocksY * blockSize;
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

		// Read header
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

		// Check for DX10 extension
		DDSHeaderDXT10 dxt10{};
		DDSHeaderDXT10* pDXT10 = nullptr;
		if (header.pixelFormat.fourCC == FOURCC_DX10)
		{
			file.read(reinterpret_cast<char*>(&dxt10), sizeof(DDSHeaderDXT10));
			pDXT10 = &dxt10;
		}

		// Determine the BC format
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

		// Get texture dimensions and mip count
		uint32_t width = header.width;
		uint32_t height = header.height;
		uint32_t mipCount = (header.flags & DDSD_MIPMAPCOUNT) ? header.mipMapCount : 1;

		// Populate base texture info
		const std::string filenameStr = filePath.filename().string();
		result.pName = new char[filenameStr.size() + 1];
		std::strcpy(result.pName, filenameStr.c_str());
		result.size = { width, height };
		result.format = format;
		result.bytesPerPixel = 0; // Compressed
		result.mipLevels.resize(mipCount);

		// Read each mip level
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
				result.pData = pMipData; // mip 0 is also accessible via pData
			}

			curWidth = (curWidth > 1) ? curWidth / 2 : 1;
			curHeight = (curHeight > 1) ? curHeight / 2 : 1;
		}

		std::cout << "[DDS] Loaded \"" << filePath.filename().string() << "\" (" << width << "x" << height << ", " << mipCount << " mips)" << std::endl;

		return result;
	}
}
