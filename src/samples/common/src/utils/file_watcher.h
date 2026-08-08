#pragma once

#include <string>
#include <vector>

namespace Common
{
	class FileWatcherImpl;

	// Monitors directories on a background thread and reports changed files
	// to the main thread via Poll()
	// TODO - Migrate to BSL
	class FileWatcher
	{
	public:
		FileWatcher();
		~FileWatcher();

		FileWatcher(const FileWatcher&) = delete;
		FileWatcher& operator=(const FileWatcher&) = delete;

		// Registers a directory for monitoring. Multiple directories can be watched
		void Watch(const std::string& directory);

		// Called from the main thread each frame. Returns the list of files
		// that changed since the last poll
		void Poll(std::vector<std::string>& out_changedFiles);

	private:
		FileWatcherImpl* m_pImpl;
	};
}
