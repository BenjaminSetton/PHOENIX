#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <PHX/interface/render_device.h>
#include <PHX/interface/shader.h>
#include <PHX/types/shader_desc.h>

#include "file_watcher.h"
#include "shader_utils.h"

namespace Common
{
	class ShaderManager
	{
	public:
		ShaderManager();
		~ShaderManager();

		ShaderManager(const ShaderManager&) = delete;
		ShaderManager& operator=(const ShaderManager&) = delete;

		// The returned handle stays valid for the lifetime of the shader, since we
		// update the data underneath the same handle
		PHX::ShaderHandle RegisterShader(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths = GetCommonShaderIncludePath());

		void PollUpdates();

	private:
		struct ShaderEntry
		{
			std::string filePath;
			PHX::SHADER_STAGE stage = PHX::SHADER_STAGE::MAX;
			std::vector<std::string> includePaths;
			std::vector<std::string> resolvedIncludes; // Full paths of all #include'd files
			PHX::ShaderHandle handle = PHX::INVALID_HANDLE;
		};

		// Reads a file and returns its contents as a string. Returns false on failure
		bool ReadFile(const std::string& path, std::string& out_content);

		// Scans source text for #include "filename" directives, and returns full 
		// paths of all included files
		std::vector<std::string> ParseIncludes(const std::string& source, const std::string& baseDir, const std::vector<std::string>& searchPaths);

		// Compiles shader source and allocates (or reloads) the shader handle
		bool CompileAndAllocate(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths, PHX::ShaderHandle& handle);

		// Returns the directory portion of a file path
		static std::string GetDirectory(const std::string& filePath);

		// Registers a directory with the file watcher if not already watched
		void RegisterWatchDir(const std::string& dir);

	private:
	
		FileWatcher m_fileWatcher;
		PHX::RenderDeviceHandle m_device;

		std::vector<ShaderEntry> m_shaders;

		// Maps include file paths to indices of shaders that depend on them
		std::unordered_map<std::string, std::vector<size_t>> m_includeDependencyMap;

		// Tracks which directories are already being watched
		std::unordered_set<std::string> m_watchedDirs;
	};
}
