#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace Common
{
	struct FileHeader
	{
		char     magic[4];        // "SMPA" for assets, "SMPT" for textures
		uint32_t version;         // Format version
		uint32_t typeHash;        // Hash of type name — sanity check on load
	};

	static constexpr uint32_t ASSET_FORMAT_VERSION   = 5;
	static constexpr uint32_t TEXTURE_FORMAT_VERSION = 1;

	static constexpr const char* ASSET_MAGIC   = "SMPA";
	static constexpr const char* TEXTURE_MAGIC = "SMPT";

	// Simple compile-time string hash for type identification
	static constexpr uint32_t HashTypeName(const char* str, uint32_t hash = 2166136261u)
	{
		return (str == nullptr || *str == '\0') ? hash : HashTypeName(str + 1, (hash ^ static_cast<uint32_t>(*str)) * 16777619u);
	}

	inline bool WriteHeader(std::ostream& os, const char* magic, uint32_t version, uint32_t typeHash)
	{
		FileHeader header{};
		memcpy(header.magic, magic, 4);
		header.version   = version;
		header.typeHash  = typeHash;

		os.write(reinterpret_cast<const char*>(&header), sizeof(FileHeader));
		return os.good();
	}

	inline bool ReadAndValidateHeader(std::istream& is, const char* expectedMagic, uint32_t expectedVersion, uint32_t expectedTypeHash)
	{
		FileHeader header{};
		is.read(reinterpret_cast<char*>(&header), sizeof(FileHeader));
		if (!is.good())
		{
			std::cout << "[ASSET] Failed to read file header" << std::endl;
			return false;
		}

		if (memcmp(header.magic, expectedMagic, 4) != 0)
		{
			std::cout << "[ASSET] Magic mismatch, expected '" << expectedMagic << "', got '"
			          << std::string(header.magic, 4) << "'" << std::endl;
			return false;
		}

		if (header.version != expectedVersion)
		{
			std::cout << "[ASSET] Version mismatch, expected " << expectedVersion << ", got " << header.version << std::endl;
			return false;
		}

		if (header.typeHash != expectedTypeHash)
		{
			std::cout << "[ASSET] Type hash mismatch, expected " << expectedTypeHash << ", got " << header.typeHash << std::endl;
			return false;
		}

		return true;
	}

	// Helper functions for writing/reading trivial types to binary streams
	template<typename T>
	inline void WriteTrivial(std::ostream& os, const T& value)
	{
		os.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}

	template<typename T>
	inline T ReadTrivial(std::istream& is)
	{
		T value{};
		is.read(reinterpret_cast<char*>(&value), sizeof(T));
		return value;
	}

	inline void WriteString(std::ostream& os, const std::string& str)
	{
		uint32_t length = static_cast<uint32_t>(str.size());
		WriteTrivial(os, length);
		os.write(str.data(), length);
	}

	inline std::string ReadString(std::istream& is)
	{
		uint32_t length = ReadTrivial<uint32_t>(is);
		std::string str(length, '\0');
		is.read(&str[0], length);
		return str;
	}

	inline void WriteBytes(std::ostream& os, const uint8_t* data, uint64_t size)
	{
		WriteTrivial(os, size);
		os.write(reinterpret_cast<const char*>(data), size);
	}

	inline std::vector<uint8_t> ReadBytes(std::istream& is)
	{
		uint64_t size = ReadTrivial<uint64_t>(is);
		std::vector<uint8_t> data(size);
		is.read(reinterpret_cast<char*>(data.data()), size);
		return data;
	}
}
