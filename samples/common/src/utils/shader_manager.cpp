
#include <algorithm>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>

#include <PHX/phx.h>

#include "shader_manager.h"

#include "shader_utils.h"

namespace Common
{
	ShaderManager::ShaderManager()
	{
	}

	ShaderManager::~ShaderManager()
	{
	}

	std::string ShaderManager::GetDirectory(const std::string& filePath)
	{
		size_t pos = filePath.find_last_of("/\\");
		if (pos == std::string::npos)
		{
			return ".";
		}
		return filePath.substr(0, pos);
	}

	bool ShaderManager::ReadFile(const std::string& path, std::string& out_content)
	{
		std::ifstream file(path, std::ios::in);
		if (!file.is_open())
		{
			return false;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		out_content = buffer.str();
		return true;
	}

	std::vector<std::string> ShaderManager::ParseIncludes(const std::string& source, const std::string& baseDir, const std::vector<std::string>& searchPaths)
	{
		std::vector<std::string> result;
		std::unordered_set<std::string> visited;

		// Recursive lambda for parsing includes
		std::function<void(const std::string&, const std::string&)> parseRecursive =
			[&](const std::string& src, const std::string& currDir)
		{
			std::istringstream stream(src);
			std::string line;
			while (std::getline(stream, line))
			{
				// Find #include "..." pattern
				size_t includePos = line.find("#include");
				if (includePos == std::string::npos)
					continue;

				size_t quoteStart = line.find('"', includePos);
				if (quoteStart == std::string::npos)
					continue;

				size_t quoteEnd = line.find('"', quoteStart + 1);
				if (quoteEnd == std::string::npos)
					continue;

				std::string includeName = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);

				// Try to resolve the include path
				std::string resolvedPath;
				bool found = false;

				// First, search relative to the current directory
				{
					std::string candidate = currDir + "/" + includeName;
					std::ifstream testFile(candidate);
					if (testFile.is_open())
					{
						resolvedPath = candidate;
						found = true;
					}
				}

				// Then, search in the provided search paths
				if (!found)
				{
					for (const std::string& searchPath : searchPaths)
					{
						std::string candidate = searchPath + "/" + includeName;
						std::ifstream testFile(candidate);
						if (testFile.is_open())
						{
							resolvedPath = candidate;
							found = true;
							break;
						}
					}
				}

				if (!found)
					continue;

				if (visited.count(resolvedPath) > 0)
					continue;

				visited.insert(resolvedPath);
				result.push_back(resolvedPath);

				// Recursively parse the included file
				std::string includeContent;
				if (ReadFile(resolvedPath, includeContent))
				{
					std::string includeDir = GetDirectory(resolvedPath);
					parseRecursive(includeContent, includeDir);
				}
			}
		};

		parseRecursive(source, baseDir);
		return result;
	}

	bool ShaderManager::CompileAndAllocate(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths, PHX::ShaderHandle& handle)
	{
		std::string source;
		if (!ReadFile(filePath, source))
		{
			return false;
		}

		// Build include paths
		std::vector<const char*> includePathPtrs;
		includePathPtrs.reserve(includePaths.size());
		for (const std::string& path : includePaths)
		{
			includePathPtrs.push_back(path.c_str());
		}

		PHX::ShaderSourceData shaderSrc{};
		shaderSrc.data = source.c_str();
		shaderSrc.entryPoint = "main";
		shaderSrc.stage = stage;
		shaderSrc.origin = GetOriginFromFilePath(filePath);
		shaderSrc.includePaths = includePathPtrs.data();
		shaderSrc.includePathCount = static_cast<PHX::u32>(includePathPtrs.size());
		shaderSrc.performReflection = true;

		PHX::CompiledShader compiled;
		if (PHX::CompileShader(shaderSrc, compiled) != PHX::STATUS_CODE::SUCCESS)
		{
			return false;
		}

		PHX::ShaderCreateInfo createInfo{};
		createInfo.pBytecode = compiled.data.get();
		createInfo.size = compiled.size;
		createInfo.stage = stage;
		createInfo.reflectionData = compiled.reflectionData;

		if (handle.IsEmpty())
		{
			return (device.AllocateShader(createInfo, handle) == PHX::STATUS_CODE::SUCCESS);
		}
		else
		{
			return (device.ReloadShader(createInfo, handle) == PHX::STATUS_CODE::SUCCESS);
		}
	}

	void ShaderManager::RegisterWatchDir(const std::string& dir)
	{
		if (m_watchedDirs.count(dir) > 0)
		{
			return;
		}

		std::cout << "Watching directory: \"" << dir.c_str() << "\"" << std::endl;

		m_watchedDirs.insert(dir);
		m_fileWatcher.Watch(dir);
	}

	PHX::ShaderHandle ShaderManager::RegisterShader(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths)
	{
		m_device = device;

		ShaderEntry entry;
		entry.filePath = filePath;
		entry.stage = stage;
		entry.includePaths = includePaths;
		entry.handle = PHX::ShaderHandle{};

		// Read and parse includes
		std::string source;
		if (!ReadFile(filePath, source))
		{
			return PHX::INVALID_HANDLE;
		}

		std::string baseDir = GetDirectory(filePath);
		entry.resolvedIncludes = ParseIncludes(source, baseDir, includePaths);

		// Compile and allocate
		if (!CompileAndAllocate(filePath, stage, device, includePaths, entry.handle))
		{
			return PHX::INVALID_HANDLE;
		}

		size_t index = m_shaders.size();

		// Register directories with the file watcher
		RegisterWatchDir(baseDir);
		for (const std::string& includePath : entry.resolvedIncludes)
		{
			RegisterWatchDir(GetDirectory(includePath));
		}

		// Build include dependency map
		for (const std::string& includeFile : entry.resolvedIncludes)
		{
			m_includeDependencyMap[includeFile].push_back(index);
		}

		m_shaders.push_back(std::move(entry));

		return m_shaders[index].handle;
	}

	void ShaderManager::PollUpdates()
	{
		// Step 1: Detect changes
		std::vector<std::string> changedFiles;
		m_fileWatcher.Poll(changedFiles);

		if (changedFiles.empty())
			return;

		// Step 2: Collect affected shader indices
		std::unordered_set<size_t> affectedShaders;

		for (const std::string& changedFile : changedFiles)
		{
			// Check if a main shader file changed
			for (size_t i = 0; i < m_shaders.size(); i++)
			{
				if (m_shaders[i].filePath == changedFile)
				{
					affectedShaders.insert(i);
				}
			}

			// Check if an include file changed
			auto depIt = m_includeDependencyMap.find(changedFile);
			if (depIt != m_includeDependencyMap.end())
			{
				for (size_t shaderIdx : depIt->second)
				{
					affectedShaders.insert(shaderIdx);
				}
			}
		}

		if (affectedShaders.empty())
			return;

		// Step 3: Recompile affected shaders
		bool anyReloaded = false;

		for (size_t shaderIdx : affectedShaders)
		{
			ShaderEntry& entry = m_shaders[shaderIdx];

			// Re-read source and re-parse includes
			std::string source;
			if (!ReadFile(entry.filePath, source))
			{
				continue;
			}

			std::string baseDir = GetDirectory(entry.filePath);
			std::vector<std::string> newIncludes = ParseIncludes(source, baseDir, entry.includePaths);

			// Update dependency map: remove old include dependencies
			for (const std::string& oldInclude : entry.resolvedIncludes)
			{
				auto& deps = m_includeDependencyMap[oldInclude];
				deps.erase(std::remove(deps.begin(), deps.end(), shaderIdx), deps.end());
				if (deps.empty())
				{
					m_includeDependencyMap.erase(oldInclude);
				}
			}

			// Update dependency map: add new include dependencies
			for (const std::string& newInclude : newIncludes)
			{
				m_includeDependencyMap[newInclude].push_back(shaderIdx);
			}

			entry.resolvedIncludes = newIncludes;

			// Recompile and reload
			if (CompileAndAllocate(entry.filePath, entry.stage, m_device, entry.includePaths, entry.handle))
			{
				anyReloaded = true;
			}
		}

		// Step 4: Flush pipeline cache (FlushPipelineCache calls vkDeviceWaitIdle internally)
		if (anyReloaded)
		{
			m_device.FlushPipelineCache();
		}
	}
}
