#pragma once

#include <filesystem>
#include <stdint.h>
#include <unordered_map>

#include "utils/uuid.h"

namespace Common
{
	using AssetHandle = UUID;

	static constexpr AssetHandle INVALID_ASSET_HANDLE = INVALID_UUID;
	static constexpr uint32_t MAX_HANDLE_GENERATION_RETRIES = 50;

	template<typename AssetType>
	class GenericAssetManager
	{
	public:

		static GenericAssetManager& Get()
		{
			static GenericAssetManager instance;
			return instance;
		}

		AssetHandle LoadModel(std::filesystem::path filePath)
		{
			// TODO
			return INVALID_ASSET_HANDLE;
		}

		AssetType* GetAsset(AssetHandle id)
		{
			if (m_assets.find(id) == m_assets.end())
			{
				return nullptr;
			}
			return &m_assets.at(id);
		}

		const AssetType* const GetAsset(AssetHandle id) const
		{
			if (m_assets.find(id) == m_assets.end())
			{
				return nullptr;
			}
			return &m_assets.at(id);
		}
		
		AssetHandle AddAsset(const AssetType& asset)
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

			// Copy AssetType into container
			m_assets.insert({ id, asset });
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

		std::unordered_map<AssetHandle, AssetType> m_assets;
	};
}