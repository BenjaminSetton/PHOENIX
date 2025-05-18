
#include <iostream>
#include <cstdarg>
#include <time.h>

#include "logger.h"

#include "PHX/types/integral_types.h"

#define LOG_FORMAT_CODE(logType) \
	va_list va; \
	va_start(va, format); \
	vsprintf_s(s_printBuffer, MAX_BUFFER_SIZE_BYTES, format, va); \
	Log_Internal(logType, s_printBuffer); \
	va_end(va);

namespace PHX
{
	static constexpr u32 MAX_BUFFER_SIZE_BYTES = 5000;
	static char s_printBuffer[MAX_BUFFER_SIZE_BYTES];

	// ANSI color codes
	static const char* LogErrorColor   = "\x1B[31m";
	static const char* LogWarningColor = "\x1B[33m";
	static const char* LogInfoColor    = "\x1B[37m";
	static const char* LogDebugColor   = "\x1B[36m";
	static const char* LogClearColor   = "\033[0m";

	// Log type buffers
	static const char* LogErrorBuffer   = "ERROR";
	static const char* LogWarningBuffer = "WARNING";
	static const char* LogInfoBuffer    = "INFO";
	static const char* LogDebugBuffer   = "DEBUG";

	// Define simple logging functions
	static void Log_Internal(LOG_TYPE type, const char* message)
	{
		auto& settings = GetSettings();
		if (settings.logCallback != nullptr)
		{
			settings.logCallback(message, type);
		}
		else
		{
			const char* logTypeColor = nullptr;
			const char* logTypeBuffer = nullptr;

			switch (type)
			{
			case LOG_TYPE::DBG:
			{
				logTypeColor = LogDebugColor;
				logTypeBuffer = LogDebugBuffer;
				break;
			}
			case LOG_TYPE::INFO:
			{
				logTypeColor = LogInfoColor;
				logTypeBuffer = LogInfoBuffer;
				break;
			}
			case LOG_TYPE::WARNING:
			{
				logTypeColor = LogWarningColor;
				logTypeBuffer = LogWarningBuffer;
				break;
			}
			case LOG_TYPE::ERR:
			{
				logTypeColor = LogErrorColor;
				logTypeBuffer = LogErrorBuffer;
				break;
			}
			}

			// Get the timestamp
			std::time_t currentTime = std::time(nullptr);
			std::tm currentLocalTime{};
			localtime_s(&currentLocalTime, &currentTime);

			int hour = currentLocalTime.tm_hour;
			char AMorPM[3] = "AM"; // Contains "AM" or "PM" including null terminator

			if (hour > 12)
			{
				hour -= 12;
				memcpy(AMorPM, "PM", 3);
			}

			fprintf_s(stderr, "[PHOENIX][%02d:%02d:%02d %s]%s[%s] %s %s\n",
				hour,
				currentLocalTime.tm_min,
				currentLocalTime.tm_sec,
				AMorPM,
				logTypeColor,
				logTypeBuffer,
				message,
				LogClearColor);
		}
	}

	void LogError(const char* format, ...)
	{
		LOG_FORMAT_CODE(LOG_TYPE::ERR);
	}

	void LogWarning(const char* format, ...)
	{
		LOG_FORMAT_CODE(LOG_TYPE::WARNING);
	}

	void LogInfo(const char* format, ...)
	{
		LOG_FORMAT_CODE(LOG_TYPE::INFO);
	}

	void LogDebug(const char* format, ...)
	{
#if defined(PHX_DEBUG)
		LOG_FORMAT_CODE(LOG_TYPE::DBG);
#elif
		UNUSED(format);
#endif
	}
}
