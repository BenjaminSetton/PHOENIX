#include "file_watcher.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>

namespace Common
{
	static double GetTimeSeconds()
	{
		static auto startTime = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed = now - startTime;
		return elapsed.count();
	}

	class FileWatcherImpl
	{
	public:
		FileWatcherImpl();
		~FileWatcherImpl();

		void Watch(const std::string& directory);
		void Poll(std::vector<std::string>& out_changedFiles);

	private:
		struct WatchEntry
		{
			HANDLE hDirectory = INVALID_HANDLE_VALUE;
			OVERLAPPED overlapped{};
			BYTE buffer[4096]{};
			std::string dirPath;
			bool pending = false;
		};

		void WatchThreadFunc();
		void ProcessNotification(WatchEntry& entry, const FILE_NOTIFY_INFORMATION* pInfo);

		std::vector<WatchEntry> m_watchEntries;
		std::thread m_watchThread;
		std::atomic<bool> m_running{false};

		std::mutex m_changedFilesMutex;
		std::unordered_map<std::string, double> m_pendingChanges;
	};

	FileWatcherImpl::FileWatcherImpl()
	{
	}

	FileWatcherImpl::~FileWatcherImpl()
	{
		m_running = false;

		for (WatchEntry& entry : m_watchEntries)
		{
			if (entry.hDirectory != INVALID_HANDLE_VALUE)
			{
				CancelIoEx(entry.hDirectory, &entry.overlapped);
				CloseHandle(entry.hDirectory);
				entry.hDirectory = INVALID_HANDLE_VALUE;
			}
		}

		if (m_watchThread.joinable())
		{
			m_watchThread.join();
		}

		for (WatchEntry& entry : m_watchEntries)
		{
			if (entry.overlapped.hEvent != nullptr)
			{
				CloseHandle(entry.overlapped.hEvent);
				entry.overlapped.hEvent = nullptr;
			}
		}
	}

	void FileWatcherImpl::Watch(const std::string& directory)
	{
		WatchEntry entry;
		entry.dirPath = directory;

		if (!entry.dirPath.empty() && (entry.dirPath.back() == '/' || entry.dirPath.back() == '\\'))
		{
			entry.dirPath.pop_back();
		}

		entry.hDirectory = CreateFileA(
			entry.dirPath.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr
		);

		if (entry.hDirectory == INVALID_HANDLE_VALUE)
		{
			return;
		}

		entry.overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		if (entry.overlapped.hEvent == nullptr)
		{
			CloseHandle(entry.hDirectory);
			entry.hDirectory = INVALID_HANDLE_VALUE;
			return;
		}

		m_watchEntries.push_back(std::move(entry));

		if (!m_running)
		{
			m_running = true;
			m_watchThread = std::thread(&FileWatcherImpl::WatchThreadFunc, this);
		}
	}

	void FileWatcherImpl::Poll(std::vector<std::string>& out_changedFiles)
	{
		out_changedFiles.clear();

		double now = GetTimeSeconds();
		const double debounceThreshold = 0.1;

		std::lock_guard<std::mutex> lock(m_changedFilesMutex);

		for (auto it = m_pendingChanges.begin(); it != m_pendingChanges.end(); )
		{
			if (now - it->second >= debounceThreshold)
			{
				out_changedFiles.push_back(it->first);
				it = m_pendingChanges.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void FileWatcherImpl::WatchThreadFunc()
	{
		while (m_running)
		{
			// Issue ReadDirectoryChangesW for any entries that don't have a pending read.
			// This handles entries added after the thread started.
			for (WatchEntry& entry : m_watchEntries)
			{
				if (!entry.pending && entry.hDirectory != INVALID_HANDLE_VALUE)
				{
					DWORD bytesReturned = 0;
					if (ReadDirectoryChangesW(
						entry.hDirectory,
						entry.buffer,
						sizeof(entry.buffer),
						TRUE,
						FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
						&bytesReturned,
						&entry.overlapped,
						nullptr))
					{
						entry.pending = true;
					}
				}
			}

			// Collect event handles for all entries with pending reads
			std::vector<HANDLE> events;
			std::vector<size_t> eventIndices;
			for (size_t i = 0; i < m_watchEntries.size(); i++)
			{
				if (m_watchEntries[i].pending)
				{
					events.push_back(m_watchEntries[i].overlapped.hEvent);
					eventIndices.push_back(i);
				}
			}

			if (events.empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			DWORD waitResult = WaitForMultipleObjects(
				static_cast<DWORD>(events.size()),
				events.data(),
				FALSE,
				100
			);

			if (!m_running)
			{
				break;
			}

			if (waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED)
			{
				continue;
			}

			DWORD eventIndex = waitResult - WAIT_OBJECT_0;
			if (eventIndex >= events.size())
			{
				continue;
			}

			size_t entryIndex = eventIndices[eventIndex];
			WatchEntry& entry = m_watchEntries[entryIndex];
			ResetEvent(entry.overlapped.hEvent);

			DWORD bytesTransferred = 0;
			if (!GetOverlappedResult(entry.hDirectory, &entry.overlapped, &bytesTransferred, FALSE))
			{
				entry.pending = false;
				continue;
			}

			entry.pending = false;

			if (bytesTransferred > 0)
			{
				const FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(entry.buffer);
				ProcessNotification(entry, pInfo);
			}
		}
	}

	void FileWatcherImpl::ProcessNotification(WatchEntry& entry, const FILE_NOTIFY_INFORMATION* pInfo)
	{
		while (true)
		{
			if (pInfo->Action == FILE_ACTION_MODIFIED || pInfo->Action == FILE_ACTION_RENAMED_NEW_NAME)
			{
				int len = WideCharToMultiByte(
					CP_UTF8, 0,
					pInfo->FileName,
					pInfo->FileNameLength / sizeof(WCHAR),
					nullptr, 0,
					nullptr, nullptr
				);

				std::string filename(len, '\0');
				WideCharToMultiByte(
					CP_UTF8, 0,
					pInfo->FileName,
					pInfo->FileNameLength / sizeof(WCHAR),
					filename.data(), len,
					nullptr, nullptr
				);

				std::string fullPath = entry.dirPath + "/" + filename;

				double now = GetTimeSeconds();
				std::lock_guard<std::mutex> lock(m_changedFilesMutex);
				m_pendingChanges[fullPath] = now;
			}

			if (pInfo->NextEntryOffset == 0)
			{
				break;
			}

			pInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
				reinterpret_cast<const BYTE*>(pInfo) + pInfo->NextEntryOffset
			);
		}
	}

	// --- FileWatcher public interface ---

	FileWatcher::FileWatcher()
	{
		m_pImpl = new FileWatcherImpl();
	}

	FileWatcher::~FileWatcher()
	{
		delete m_pImpl;
	}

	void FileWatcher::Watch(const std::string& directory)
	{
		m_pImpl->Watch(directory);
	}

	void FileWatcher::Poll(std::vector<std::string>& out_changedFiles)
	{
		m_pImpl->Poll(out_changedFiles);
	}
}
