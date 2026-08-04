#pragma once

#include <fstream>
#include <filesystem>

#include "utils/asset_importer.h"

namespace Common
{
	// Trait struct that each sample must specialize for its AssetType.
	// Fails at compile-time if used without a specialization.
	//
	// Each specialization must provide:
	//   static constexpr uint32_t TypeHash;  // Unique compile-time hash for type identification
	//   static void Write(std::ostream& os, const T& asset, const std::filesystem::path& outputDir);
	//   static T*   Read(std::istream& is, const std::filesystem::path& inputDir);
	//   static T*   FromDisk(const AssetDisk& disk);
	//
	// Write:  Serializes T to a binary stream (called by AssetManager during cache-write).
	//         outputDir is the directory where the .asset file lives — .tex files go alongside it.
	// Read:   Deserializes T from a binary stream (called by AssetManager during cache-load).
	//         inputDir is the directory where the .asset file lives — .tex files are read from it.
	//         Returns a heap-allocated T* (ownership transfers to AssetManager).
	// FromDisk: Converts AssetDisk -> T (called by AssetManager during import).
	//           Returns a heap-allocated T* (ownership transfers to AssetManager).
	template<typename T>
	struct Serializer; // No default — must specialize per type
}
