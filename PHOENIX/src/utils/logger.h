#pragma once

namespace PHX
{
	enum class LogType
	{
		DEBUG = -1,
		INFO,
		WARNING,
		ERR,
	};

	void LogError(const char* format, ...);
	void LogWarning(const char* format, ...);
	void LogInfo(const char* format, ...);
}