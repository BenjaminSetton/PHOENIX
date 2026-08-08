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
		PHX::ShaderHandle LoadShader(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths = GetCommonShaderIncludePath());

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

		// Loads or compiles the shader via LoadOrCompileShader and allocates (or reloads) the handle.
		// If out_resolvedIncludes is non-null, fills it with the main file + all transitive include paths.
		bool CompileAndAllocate(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths, PHX::ShaderHandle& handle, PHX::ResolvedShaderIncludes* out_resolvedIncludes = nullptr);

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
