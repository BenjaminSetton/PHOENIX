#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>

#include "utils/uuid.h"
#include "utils/asset_importer.h"
#include "serializer.h"
#include "asset_file_format.h"

#ifndef COMMON_ASSET_ROOT_DIR
	#define COMMON_ASSET_ROOT_DIR "."
#endif

#ifndef SAMPLE_ASSET_ROOT_DIR
	#define SAMPLE_ASSET_ROOT_DIR "."
#endif

#ifndef CACHE_ROOT_DIR
	#define CACHE_ROOT_DIR "."
#endif

namespace Common
{
	using AssetHandle = UUID;

	static constexpr AssetHandle INVALID_ASSET_HANDLE = INVALID_UUID;
	static constexpr uint32_t MAX_HANDLE_GENERATION_RETRIES = 50;

	template<typename AssetType>
	class AssetManager
	{
	public:

		static AssetManager& Get()
		{
			static AssetManager instance;
			return instance;
		}

		// Automatic: check cache -> load if valid, else import + serialize. Can fail and return INVALID_HANDLE
		AssetHandle LoadOrImport(const std::filesystem::path& sourcePath, bool forceReimport = false, bool importAnimations = false)
		{
			std::filesystem::path fullSourcePath = FindSourceFile(sourcePath);
			if (fullSourcePath.empty())
			{
				std::cout << "[ASSET] Could not load or import asset '" << sourcePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			std::filesystem::path cachePath = ComputeCachePath(sourcePath);

			if (!forceReimport && IsCacheValid(cachePath, fullSourcePath))
			{
				return Load(cachePath);
			}

			return Import(sourcePath, importAnimations);
		}

		// Force import from source file (re-imports, serializes, stores). Can fail and return INVALID_HANDLE
		AssetHandle Import(const std::filesystem::path& sourcePath, bool importAnimations = false)
		{
			std::filesystem::path fullSourcePath = FindSourceFile(sourcePath);
			if (fullSourcePath.empty())
			{
				std::cout << "[ASSET] Could not import asset '" << sourcePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			std::cout << "[ASSET] Importing asset: '" << sourcePath << "'" << std::endl;

			std::shared_ptr<AssetDisk> pAssetDisk = ImportAsset(fullSourcePath, importAnimations);
			if (pAssetDisk == nullptr)
			{
				std::cout << "[ASSET] Failed to import asset from '" << fullSourcePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			AssetType* pAsset = Serializer<AssetType>::FromDisk(*pAssetDisk);

			std::filesystem::path cachePath = ComputeCachePath(sourcePath);
			SerializeToDisk(cachePath, *pAsset, fullSourcePath);

			std::cout << "[ASSET] Finished importing asset: '" << cachePath << "'" << std::endl;
			return AddAsset(std::unique_ptr<AssetType>(pAsset));
		}

		// Load from serialized cache file only. Can fail and return INVALID_HANDLE
		AssetHandle Load(const std::filesystem::path& cachePath)
		{
			if (!std::filesystem::exists(cachePath))
			{
				std::cout << "[ASSET] Cache file does not exist: '" << cachePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			std::cout << "[ASSET] Loading asset: '" << cachePath << "'" << std::endl;

			constexpr size_t READ_BUFFER_SIZE = 512 * 1024; // Use a 512KB read buffer because we generally load large assets for samples
			std::vector<char> readBuffer(READ_BUFFER_SIZE);

			std::ifstream file;
			file.rdbuf()->pubsetbuf(readBuffer.data(), static_cast<std::streamsize>(READ_BUFFER_SIZE));
			file.open(cachePath, std::ios::binary);
			if (!file.is_open())
			{
				std::cout << "[ASSET] Failed to open cache file: '" << cachePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			if (!BSL::ReadAndValidateHeader(file, ASSET_MAGIC, ASSET_FORMAT_VERSION, Serializer<AssetType>::TypeHash))
			{
				std::cout << "[ASSET] Cache file header validation failed: '" << cachePath << "'" << std::endl;
				return INVALID_ASSET_HANDLE;
			}

			std::filesystem::path inputDir = cachePath.parent_path();
			AssetType* pAsset = Serializer<AssetType>::Read(file, inputDir);

			std::cout << "[ASSET] Finished loading asset: '" << cachePath << "'" << std::endl;
			return AddAsset(std::unique_ptr<AssetType>(pAsset));
		}

		AssetType* GetAsset(AssetHandle id)
		{
			auto it = m_assets.find(id);
			if (it == m_assets.end())
			{
				return nullptr;
			}
			return it->second.get();
		}

		const AssetType* GetAsset(AssetHandle id) const
		{
			auto it = m_assets.find(id);
			if (it == m_assets.end())
			{
				return nullptr;
			}
			return it->second.get();
		}

		AssetHandle AddAsset(std::unique_ptr<AssetType> asset)
		{
			AssetHandle id = INVALID_ASSET_HANDLE;
			uint32_t currAttempt = 0;
			do
			{
				id = GetUUID();
				if (m_assets.find(id) == m_assets.end())
				{
					break;
				}

			} while (currAttempt < MAX_HANDLE_GENERATION_RETRIES);

			m_assets.emplace(id, std::move(asset));
			return id;
		}

		void RemoveAsset(AssetHandle id)
		{
			const auto iter = m_assets.find(id);
			if (iter != m_assets.end())
			{
				m_assets.erase(iter);
			}
		}

	private:

		std::filesystem::path FindSourceFile(const std::filesystem::path& relativePath) const
		{
			std::filesystem::path commonPath = std::filesystem::path(COMMON_ASSET_ROOT_DIR) / relativePath;
			if (std::filesystem::exists(commonPath))
			{
				return commonPath;
			}

			std::filesystem::path samplePath = std::filesystem::path(SAMPLE_ASSET_ROOT_DIR) / relativePath;
			if (std::filesystem::exists(samplePath))
			{
				return samplePath;
			}

			if (std::filesystem::exists(relativePath))
			{
				return relativePath;
			}

			std::cout << "[ASSET] Could not find source asset '" << relativePath << "' in any asset root" << std::endl;
			return {};
		}

		std::filesystem::path ComputeCachePath(const std::filesystem::path& relativePath) const
		{
			std::filesystem::path cachePath = std::filesystem::path(CACHE_ROOT_DIR) / std::filesystem::path("scene") / relativePath;
			cachePath.replace_extension(".asset");
			return cachePath;
		}

		bool IsCacheValid(const std::filesystem::path& cachePath, const std::filesystem::path& sourcePath) const
		{
			(void)sourcePath;

			if (!std::filesystem::exists(cachePath))
			{
				return false;
			}

			std::ifstream file(cachePath, std::ios::binary);
			if (!file.is_open())
			{
				return false;
			}

			BSL::FileHeader header{};
			file.read(reinterpret_cast<char*>(&header), sizeof(BSL::FileHeader));
			if (!file.good())
			{
				return false;
			}

			if (std::memcmp(header.magic, ASSET_MAGIC, 4) != 0)
			{
				return false;
			}

			if (header.version != ASSET_FORMAT_VERSION)
			{
				return false;
			}

			if (header.typeHash != Serializer<AssetType>::TypeHash)
			{
				return false;
			}

			return true;
		}

		void SerializeToDisk(const std::filesystem::path& cachePath, const AssetType& asset, const std::filesystem::path& sourcePath) const
		{
			(void)sourcePath;

			std::filesystem::path parentDir = cachePath.parent_path();
			if (!parentDir.empty() && !std::filesystem::exists(parentDir))
			{
				std::filesystem::create_directories(parentDir);
			}

			std::ofstream file(cachePath, std::ios::binary);
			if (!file.is_open())
			{
				std::cout << "[ASSET] Failed to create cache file: '" << cachePath << "'" << std::endl;
				return;
			}

			BSL::WriteHeader(file, ASSET_MAGIC, ASSET_FORMAT_VERSION, Serializer<AssetType>::TypeHash);
			Serializer<AssetType>::Write(file, asset, parentDir);

			std::cout << "[ASSET] Serialized asset: '" << cachePath << "'" << std::endl;
		}

		std::unordered_map<AssetHandle, std::unique_ptr<AssetType>> m_assets;
	};
}