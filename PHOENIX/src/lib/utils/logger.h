#pragma once

#include "utils/global_settings.h" // LOG_TYPE, GlobalSettings

namespace PHX
{
	void LogError(const char* format, ...);
	void LogWarning(const char* format, ...);
	void LogInfo(const char* format, ...);
}