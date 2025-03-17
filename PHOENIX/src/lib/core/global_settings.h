#pragma once

#include "PHX/types/settings.h"

namespace PHX
{
	class GlobalSettings
	{
	public:

		static GlobalSettings& Get()
		{
			static GlobalSettings instance;
			return instance;
		}

		const Settings& GetSettings() const;

		// This should only get called once by phx.cpp!
		void SetSettings(const Settings& settings);

	private:

		GlobalSettings();

		Settings m_settings;
		bool m_isSet;
	};

	// Utility
	const Settings& GetSettings();
}
