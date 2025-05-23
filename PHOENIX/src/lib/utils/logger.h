#pragma once

#include "core/global_settings.h" // LOG_TYPE, GlobalSettings

namespace PHX
{
	void LogError(const char* format, ...);
	void LogWarning(const char* format, ...);
	void LogInfo(const char* format, ...);
	void LogDebug(const char* format, ...);
}