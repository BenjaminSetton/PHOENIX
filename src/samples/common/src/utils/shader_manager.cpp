
#include <algorithm>
#include <iostream>

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

	bool ShaderManager::CompileAndAllocate(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths, PHX::ShaderHandle& handle, PHX::ResolvedShaderIncludes* out_resolvedIncludes)
	{
		// Build include paths
		std::vector<const char*> includePathPtrs;
		includePathPtrs.reserve(includePaths.size());
		for (const std::string& path : includePaths)
		{
			includePathPtrs.push_back(path.c_str());
		}

		PHX::ShaderFileSourceData fileSrc{};
		fileSrc.filePath = filePath.c_str();
		fileSrc.entryPoint = "main";
		fileSrc.stage = stage;
		fileSrc.includePaths = includePathPtrs.data();
		fileSrc.includePathCount = static_cast<u32>(includePathPtrs.size());
		fileSrc.performReflection = true;

		PHX::CompiledShader compiled;
		if (PHX::LoadOrCompileShader(fileSrc, compiled, out_resolvedIncludes) != PHX::STATUS_CODE::SUCCESS)
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

	PHX::ShaderHandle ShaderManager::LoadShader(const std::string& filePath, PHX::SHADER_STAGE stage, PHX::RenderDeviceHandle device, const std::vector<std::string>& includePaths)
	{
		m_device = device;

		ShaderEntry entry;
		entry.filePath = filePath;
		entry.stage = stage;
		entry.includePaths = includePaths;
		entry.handle = PHX::ShaderHandle{};

		// Load/compile — PHX returns resolved includes for file watcher registration
		PHX::ResolvedShaderIncludes resolved;
		if (!CompileAndAllocate(filePath, stage, device, includePaths, entry.handle, &resolved))
		{
			return PHX::INVALID_HANDLE;
		}

		entry.resolvedIncludes = resolved.includeFilePaths;

		size_t index = m_shaders.size();

		// Register directories with the file watcher
		RegisterWatchDir(GetDirectory(filePath));
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

			// Recompile — LoadOrCompileShader will detect content hash change (cache miss) and recompile + update cache
			PHX::ResolvedShaderIncludes newResolved;
			if (!CompileAndAllocate(entry.filePath, entry.stage, m_device, entry.includePaths, entry.handle, &newResolved))
			{
				continue;
			}

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
			for (const std::string& newInclude : newResolved.includeFilePaths)
			{
				m_includeDependencyMap[newInclude].push_back(shaderIdx);
			}

			entry.resolvedIncludes = newResolved.includeFilePaths;
			anyReloaded = true;
		}

		// Step 4: Flush pipeline cache (FlushPipelineCache calls vkDeviceWaitIdle internally)
		if (anyReloaded)
		{
			m_device.FlushPipelineCache();
		}
	}
}
