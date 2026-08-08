#pragma once

#include <cstdint>

#include "BSL/serialization.h"

namespace Common
{
	// Format versions
	static constexpr uint32_t ASSET_FORMAT_VERSION   = 5;
	static constexpr uint32_t TEXTURE_FORMAT_VERSION = 1;

	static constexpr const char* ASSET_MAGIC   = "SMPA";
	static constexpr const char* TEXTURE_MAGIC = "SMPT";

	// Simple compile-time string hash for type identification
	static constexpr uint32_t HashTypeName(const char* str, uint32_t hash = 2166136261u)
	{
		return (str == nullptr || *str == '\0') ? hash : HashTypeName(str + 1, (hash ^ static_cast<uint32_t>(*str)) * 16777619u);
	}
}
